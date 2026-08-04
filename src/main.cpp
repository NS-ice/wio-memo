#include <Arduino.h>
#include <ArduinoJson.h>
#include <DNSServer.h>
#include <RTC_SAMD51.h>
#include <Seeed_Arduino_FreeRTOS.h>
#include <Seeed_mbedtls.h>
#include <TFT_eSPI.h>
#include <WebServer.h>
#include <WiFiUdp.h>
#include <rpcWiFi.h>

#include "wio_memo/application/reminder_engine.h"
#include "wio_memo/application/task_service.h"
#include "wio_memo/application/clock.h"
#include "wio_memo/infrastructure/persistent_store.h"
#include "wio_memo/infrastructure/qspi_font.h"
#include "wio_memo/presentation/buzzer.h"
#include "wio_memo/presentation/device_ui.h"
#include "wio_memo/presentation/lvgl_port.h"
#include "wio_memo/presentation/qspi_lvgl_font.h"
#include "wio_memo/presentation/web_page.h"

#if __has_include("secrets.h")
#include "secrets.h"
#ifndef WIFI_FORCE_PROVISION
#define WIFI_FORCE_PROVISION 0
#endif
#else
#define WIFI_SSID ""
#define WIFI_PASSWORD ""
#define LOCAL_UTC_OFFSET_MINUTES 480
#define WIFI_FORCE_PROVISION 0
#endif

namespace {

constexpr uint32_t NTP_RESYNC_MS = 6UL * 60UL * 60UL * 1000UL;
constexpr uint32_t NTP_RETRY_MS = 5UL * 60UL * 1000UL;
constexpr uint32_t UI_STATUS_REFRESH_MS = 500;
constexpr uint32_t WEATHER_REFRESH_MS = 30UL * 60UL * 1000UL;
constexpr uint32_t WEATHER_RETRY_MS = 5UL * 60UL * 1000UL;
constexpr uint32_t NETWORK_STAGE_GAP_MS = 750;
constexpr uint32_t NETWORK_CONNECT_TIMEOUT_MS = 10000;
constexpr uint32_t NETWORK_RECONNECT_DELAY_MS = 5000;
constexpr uint16_t NTP_LOCAL_PORT = 2390;
constexpr uint32_t NTP_UNIX_DELTA = 2208988800UL;
constexpr uint8_t BUNDLED_NETWORK_APPLIED = 0xA8;
constexpr uint8_t START_ACCESS_POINT_ONCE = 0xA6;
constexpr bool SAFE_UI_ONLY_MODE = true;

TFT_eSPI tft;
RTC_SAMD51 rtc;
WebServer server(80);
WiFiUDP ntpUdp;
DNSServer dnsServer;
wio_memo::TaskList taskList{};
wio_memo::TaskService taskService(taskList);
wio_memo::ReminderEngine reminderEngine;
wio_memo::PersistentStore persistentStore;
wio_memo::DeviceSettings deviceSettings{};
wio_memo::QspiFont qspiFont16;
wio_memo::QspiFont qspiFont12;
wio_memo::QspiFont qspiFont20;
wio_memo::QspiLvglFont qspiLvglFont16;
wio_memo::QspiLvglFont qspiLvglFont12;
wio_memo::QspiLvglFont qspiLvglFont20;
wio_memo::LvglPort lvglPort;
wio_memo::DeviceUi deviceUi(taskList, taskService, persistentStore);
wio_memo::Buzzer buzzer(WIO_BUZZER);

bool stationMode = false;
bool apMode = false;
bool networkConnecting = false;
enum class NetworkTransition : uint8_t {
  None, StopForStation, StationStarting, StopForAccessPoint, AccessPointStarting,
  StopForOffline
};
NetworkTransition networkTransition = NetworkTransition::None;
uint32_t networkTransitionDueMs = 0;
volatile bool clockValid = false;
bool muted = false;
uint8_t selectedVisible = 0;
uint32_t lastDisplayMs = 0;
uint32_t lastNtpAttemptMs = 0;
uint32_t networkAttemptStartedMs = 0;
bool networkReconfigurePending = false;
uint32_t networkReconfigureAtMs = 0;
uint32_t lastNetworkStatusLogMs = 0;
bool webRoutesConfigured = false;
bool accessPointBootRequested = false;
bool cooperativeNetworkStarted = false;
bool weatherFetchPending = true;
uint32_t lastWeatherFetchMs = 0;
uint32_t lastCooperativeSnapshotMs = 0;
bool ntpSyncPending = false;
bool wifiScanPending = false;
enum class WeatherFetchStage : uint8_t { Idle, Location, Forecast, AirQuality, RetryWait };
WeatherFetchStage weatherFetchStage = WeatherFetchStage::Idle;
uint32_t weatherStageDueMs = 0;
float weatherLatitude = 22.5431F;
float weatherLongitude = 114.0579F;
char weatherCity[32] = "深圳";
int16_t cachedTemperature = 28;
uint8_t cachedHumidity = 65;
uint16_t cachedAqi = 0;
uint16_t cachedWindSpeed = 0;
uint8_t cachedWeatherCode = 2;

enum class NetworkCommand : uint8_t { Connect, AccessPoint, Offline };
NetworkCommand deferredNetworkCommand = NetworkCommand::Connect;
QueueHandle_t networkCommandQueue = nullptr;
SemaphoreHandle_t networkSnapshotMutex = nullptr;
struct NetworkSnapshot {
  char label[16] = "离线";
  char ip[20] = "--";
  char details[192] = "网络任务正在启动";
};
NetworkSnapshot networkSnapshot{};

String networkIp();
String formatTime(uint32_t utc, bool includeDate = false);
String formatClockTime(uint32_t utc);
void publishNetworkSnapshot();
void startWeatherFetch(uint32_t now);
void serviceWeatherFetch(uint32_t now);
void stopNetwork();
void checkReminders();

void applyBundledNetworkDefaults() {
  if (!WIFI_SSID[0]) return;
  // A bundled network is a one-time factory provision. Once the marker is stored,
  // settings changed from the web UI must survive subsequent reboots.
  if (deviceSettings.networkConfigured &&
      deviceSettings.reserved == BUNDLED_NETWORK_APPLIED) return;
  if (deviceSettings.networkConfigured && !WIFI_FORCE_PROVISION) return;
  wio_memo::DeviceSettings updated = deviceSettings;
  strlcpy(updated.stationSsid, WIFI_SSID, sizeof(updated.stationSsid));
  strlcpy(updated.stationPassword, WIFI_PASSWORD, sizeof(updated.stationPassword));
  updated.utcOffsetMinutes = LOCAL_UTC_OFFSET_MINUTES;
  updated.networkConfigured = 1;
  updated.reserved = BUNDLED_NETWORK_APPLIED;
  if (persistentStore.saveSettings(taskList, updated)) {
    deviceSettings = updated;
  } else {
    Serial.println("Failed to save bundled network settings");
  }
}

void jsonError(int code, const char *message) {
  JsonDocument doc;
  doc["error"] = message;
  String body;
  serializeJson(doc, body);
  server.send(code, "application/json; charset=utf-8", body);
}

uint32_t nowUtc() {
  return clockValid ? rtc.now().unixtime() : 0;
}

void handleGetDevice() {
  JsonDocument doc;
  doc["mode"] = stationMode ? "局域网" : (apMode ? "设备热点" :
                                          (networkConnecting ? "正在连接" : "离线"));
  doc["ip"] = networkIp();
  doc["time"] = clockValid ? formatTime(nowUtc(), true) : "";
  doc["clockSynced"] = clockValid;
  doc["weatherCity"] = weatherCity;
  doc["weatherTemperature"] = cachedTemperature;
  doc["weatherHumidity"] = cachedHumidity;
  doc["weatherReady"] = lastWeatherFetchMs != 0;
  doc["connectedSsid"] = stationMode ? WiFi.SSID() : "";
  doc["staSsid"] = deviceSettings.stationSsid;
  doc["apSsid"] = deviceSettings.accessPointSsid;
  doc["apPassword"] = deviceSettings.accessPointPassword;
  doc["utcOffsetMinutes"] = deviceSettings.utcOffsetMinutes;
  String body;
  serializeJson(doc, body);
  server.send(200, "application/json; charset=utf-8", body);
}

void handleWifiScan() {
  if (!wifiScanPending) {
    if (apMode) WiFi.mode(WIFI_AP_STA);
    WiFi.scanDelete();
    const int started = WiFi.scanNetworks(true, true);
    if (started < 0 && started != WIFI_SCAN_RUNNING) {
      return jsonError(500, "failed to start Wi-Fi scan");
    }
    wifiScanPending = true;
  }

  const int count = WiFi.scanComplete();
  if (count == WIFI_SCAN_RUNNING) {
    server.send(202, "application/json; charset=utf-8", "{\"scanning\":true}");
    return;
  }
  wifiScanPending = false;
  if (count < 0) {
    WiFi.scanDelete();
    return jsonError(500, "Wi-Fi scan failed");
  }
  JsonDocument doc;
  doc["scanning"] = false;
  JsonArray networks = doc["networks"].to<JsonArray>();
  for (int index = 0; index < count && index < 16; ++index) {
    const String ssid = WiFi.SSID(index);
    if (!ssid.length()) continue;
    bool duplicate = false;
    for (JsonObject item : networks) {
      if (item["ssid"].as<String>() == ssid) duplicate = true;
    }
    if (duplicate) continue;
    JsonObject item = networks.add<JsonObject>();
    item["ssid"] = ssid;
    item["rssi"] = WiFi.RSSI(index);
  }
  WiFi.scanDelete();
  String body;
  serializeJson(doc, body);
  server.send(200, "application/json; charset=utf-8", body);
}

void handlePutNetwork() {
  if (!server.hasArg("plain")) return jsonError(400, "missing JSON body");
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain"))) return jsonError(400, "invalid JSON");
  const char *stationSsid = doc["staSsid"] | "";
  const char *stationPassword = doc["staPassword"] | "";
  const char *apSsid = doc["apSsid"] | "";
  const char *apPassword = doc["apPassword"] | "";
  const int utcOffset = doc["utcOffsetMinutes"] | 480;
  const size_t stationSsidLength = strlen(stationSsid);
  const size_t stationPasswordLength = strlen(stationPassword);
  const size_t apSsidLength = strlen(apSsid);
  const size_t apPasswordLength = strlen(apPassword);
  if (!stationSsidLength || stationSsidLength > 32 || stationPasswordLength > 64 ||
      !apSsidLength || apSsidLength > 32 || apPasswordLength < 8 || apPasswordLength > 64 ||
      utcOffset < -720 || utcOffset > 840) {
    return jsonError(422, "invalid network settings");
  }
  wio_memo::DeviceSettings updated = deviceSettings;
  strlcpy(updated.stationSsid, stationSsid, sizeof(updated.stationSsid));
  if (stationPasswordLength) {
    strlcpy(updated.stationPassword, stationPassword, sizeof(updated.stationPassword));
  }
  strlcpy(updated.accessPointSsid, apSsid, sizeof(updated.accessPointSsid));
  strlcpy(updated.accessPointPassword, apPassword, sizeof(updated.accessPointPassword));
  updated.utcOffsetMinutes = static_cast<int16_t>(utcOffset);
  updated.networkConfigured = 1;
  if (!persistentStore.saveSettings(taskList, updated)) {
    return jsonError(500, "failed to save settings");
  }
  deviceSettings = updated;
  JsonDocument response;
  response["status"] = "saved";
  response["next"] = "connecting";
  String body;
  serializeJson(response, body);
  server.send(202, "application/json; charset=utf-8", body);
  deferredNetworkCommand = NetworkCommand::Connect;
  networkReconfigurePending = true;
  networkReconfigureAtMs = millis() + 750;
}

void handlePutNetworkMode() {
  if (!server.hasArg("plain")) return jsonError(400, "missing JSON body");
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain"))) return jsonError(400, "invalid JSON");
  const char *mode = doc["mode"] | "";
  if (strcmp(mode, "station") == 0) deferredNetworkCommand = NetworkCommand::Connect;
  else if (strcmp(mode, "ap") == 0) deferredNetworkCommand = NetworkCommand::AccessPoint;
  else if (strcmp(mode, "offline") == 0) deferredNetworkCommand = NetworkCommand::Offline;
  else return jsonError(422, "invalid network mode");
  server.send(202, "application/json; charset=utf-8", "{\"status\":\"switching\"}");
  networkReconfigurePending = true;
  networkReconfigureAtMs = millis() + 600;
}

void handleGetTasks() {
  JsonDocument doc;
  doc["now"] = nowUtc();
  doc["ip"] = stationMode ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
  doc["muted"] = muted;
  JsonArray items = doc["tasks"].to<JsonArray>();
  for (uint8_t i = 0; i < taskList.count; ++i) {
    const wio_memo::Task &task = taskList.items[i];
    JsonObject item = items.add<JsonObject>();
    item["id"] = task.id;
    item["title"] = task.title;
    item["dueUtc"] = task.startAtUtc;
    item["reminderMinutes"] = task.remindBeforeMinutes;
    item["kind"] = static_cast<uint8_t>(task.kind);
    item["completed"] = task.status == wio_memo::TaskStatus::Completed;
  }
  String body;
  serializeJson(doc, body);
  server.send(200, "application/json; charset=utf-8", body);
}

void handlePutTasks() {
  if (!server.hasArg("plain")) return jsonError(400, "missing JSON body");
  JsonDocument doc;
  const DeserializationError error = deserializeJson(doc, server.arg("plain"));
  if (error || !doc["tasks"].is<JsonArray>()) return jsonError(400, "invalid JSON");
  JsonArray items = doc["tasks"].as<JsonArray>();
  if (items.size() > wio_memo::kMaxTasks) return jsonError(422, "maximum is 12 tasks");

  wio_memo::Task incoming[wio_memo::kMaxTasks]{};
  uint8_t count = 0;
  for (JsonObject item : items) {
    const char *title = item["title"] | "";
    const uint32_t due = item["dueUtc"] | 0UL;
    const int reminder = item["reminderMinutes"] | 0;
    const int kind = item["kind"] | 0;
    if (!title[0] || strlen(title) >= wio_memo::kTaskTitleBytes || reminder < 0 ||
        (kind != 0 && kind != 1)) {
      return jsonError(422, "invalid task fields");
    }
    wio_memo::Task &target = incoming[count++];
    target.id = item["id"] | static_cast<uint32_t>(millis() + count);
    strlcpy(target.title, title, sizeof(target.title));
    target.startAtUtc = due;
    target.updatedAtUtc = nowUtc();
    target.remindBeforeMinutes = reminder;
    target.kind = kind == 1 ? wio_memo::TaskKind::Meeting : wio_memo::TaskKind::Todo;
    target.status = (item["completed"] | false) ? wio_memo::TaskStatus::Completed
                                                : wio_memo::TaskStatus::Pending;
  }
  const wio_memo::TaskList previous = taskList;
  if (taskService.replaceAll(incoming, count) != wio_memo::TaskResult::Ok) {
    return jsonError(422, "invalid task fields");
  }
  if (!persistentStore.save(taskList)) {
    taskList = previous;
    return jsonError(500, "failed to save tasks");
  }
  selectedVisible = 0;
  if (deviceUi.page() == wio_memo::UiPage::Home ||
      deviceUi.page() == wio_memo::UiPage::Tasks) {
    deviceUi.show(deviceUi.page());
  }
  server.send(204);
}

void handleWebPage() {
  constexpr size_t kPageSize = sizeof(wio_memo::kWebPage) - 1;
  constexpr size_t kChunkSize = 768;
  server.setContentLength(kPageSize);
  server.send(200, "text/html; charset=utf-8", "");
  for (size_t offset = 0; offset < kPageSize; offset += kChunkSize) {
    const size_t remaining = kPageSize - offset;
    server.sendContent_P(wio_memo::kWebPage + offset,
                         remaining < kChunkSize ? remaining : kChunkSize);
    delay(1);
  }
}

void setupWebServer() {
  server.on("/", HTTP_GET, handleWebPage);
  auto redirectToPortal = [] {
    server.sendHeader("Location", "http://192.168.5.1/", true);
    server.send(302, "text/plain; charset=utf-8", "Open Wio Memo");
  };
  server.on("/generate_204", HTTP_GET, redirectToPortal);
  server.on("/hotspot-detect.html", HTTP_GET, redirectToPortal);
  server.on("/connecttest.txt", HTTP_GET, redirectToPortal);
  server.on("/fwlink", HTTP_GET, redirectToPortal);
  server.on("/api/device", HTTP_GET, handleGetDevice);
  server.on("/api/wifi/scan", HTTP_GET, handleWifiScan);
  server.on("/api/network", HTTP_PUT, handlePutNetwork);
  server.on("/api/network/mode", HTTP_PUT, handlePutNetworkMode);
  server.on("/api/tasks", HTTP_GET, handleGetTasks);
  server.on("/api/tasks", HTTP_PUT, handlePutTasks);
  server.onNotFound([] { jsonError(404, "not found"); });
  webRoutesConfigured = true;
  if (stationMode || apMode) server.begin();
}

bool syncNtp() {
  if (!stationMode || WiFi.status() != WL_CONNECTED) return false;
  const char *hosts[] = {"ntp.aliyun.com"};
  for (const char *host : hosts) {
    IPAddress address;
    if (!WiFi.hostByName(host, address)) continue;
    uint8_t packet[48]{};
    packet[0] = 0b11100011;
    packet[2] = 6;
    packet[3] = 0xEC;
    ntpUdp.beginPacket(address, 123);
    ntpUdp.write(packet, sizeof(packet));
    ntpUdp.endPacket();
    const uint32_t started = millis();
    while (millis() - started < 900) {
      const int size = ntpUdp.parsePacket();
      if (size >= 48) {
        ntpUdp.read(packet, sizeof(packet));
        const uint32_t seconds1900 = (static_cast<uint32_t>(packet[40]) << 24) |
                                     (static_cast<uint32_t>(packet[41]) << 16) |
                                     (static_cast<uint32_t>(packet[42]) << 8) | packet[43];
        if (seconds1900 > NTP_UNIX_DELTA) {
          rtc.adjust(DateTime(seconds1900 - NTP_UNIX_DELTA));
          clockValid = true;
          return true;
        }
      }
      checkReminders();
      buzzer.update(millis());
      lvglPort.update(millis());
      delay(10);
    }
  }
  return false;
}

void serviceNtp(uint32_t now) {
  if (!stationMode || WiFi.status() != WL_CONNECTED) return;
  if (!ntpSyncPending &&
      wio_memo::ClockPolicy::intervalElapsed(now, lastNtpAttemptMs, NTP_RESYNC_MS)) {
    ntpSyncPending = true;
  }
  if (!ntpSyncPending) return;
  if (lastNtpAttemptMs != 0 &&
      !wio_memo::ClockPolicy::intervalElapsed(now, lastNtpAttemptMs, NTP_RETRY_MS)) {
    return;
  }
  lastNtpAttemptMs = now;
  if (syncNtp()) ntpSyncPending = false;
}

void startAccessPoint() {
  if (apMode && networkTransition == NetworkTransition::None) {
    publishNetworkSnapshot();
    return;
  }
  if (webRoutesConfigured) server.stop();
  dnsServer.stop();
  stationMode = false;
  apMode = false;
  networkConnecting = false;
  networkTransition = NetworkTransition::StopForAccessPoint;
  networkTransitionDueMs = millis();
  publishNetworkSnapshot();
}

void connectNetwork() {
  if (deviceSettings.networkConfigured && strlen(deviceSettings.stationSsid)) {
    if (stationMode && WiFi.status() == WL_CONNECTED &&
        WiFi.SSID() == String(deviceSettings.stationSsid)) {
      publishNetworkSnapshot();
      if (!clockValid) ntpSyncPending = true;
      return;
    }
    if (webRoutesConfigured) server.stop();
    dnsServer.stop();
    stationMode = false;
    apMode = false;
    networkConnecting = false;
    networkTransition = NetworkTransition::StopForStation;
    networkTransitionDueMs = millis();
    publishNetworkSnapshot();
  } else {
    startAccessPoint();
  }
}

void updateNetwork() {
  const uint32_t now = millis();
  if (networkReconfigurePending &&
      static_cast<int32_t>(now - networkReconfigureAtMs) >= 0) {
    networkReconfigurePending = false;
    if (deferredNetworkCommand == NetworkCommand::Connect) connectNetwork();
    else if (deferredNetworkCommand == NetworkCommand::AccessPoint) startAccessPoint();
    else stopNetwork();
  }
  if (stationMode && WiFi.status() != WL_CONNECTED) {
    if (webRoutesConfigured) server.stop();
    stationMode = false;
    networkConnecting = false;
    weatherFetchPending = false;
    weatherFetchStage = WeatherFetchStage::Idle;
    networkTransition = NetworkTransition::StopForStation;
    networkTransitionDueMs = now + NETWORK_RECONNECT_DELAY_MS;
    ntpSyncPending = true;
    publishNetworkSnapshot();
    return;
  }
  if (networkTransition == NetworkTransition::StopForStation ||
      networkTransition == NetworkTransition::StopForAccessPoint ||
      networkTransition == NetworkTransition::StopForOffline) {
    if (static_cast<int32_t>(now - networkTransitionDueMs) < 0) return;
    WiFi.mode(WIFI_OFF);
    lvglPort.update(millis());
    if (networkTransition == NetworkTransition::StopForStation) {
      networkTransition = NetworkTransition::StationStarting;
      networkTransitionDueMs = millis() + 220;
    } else if (networkTransition == NetworkTransition::StopForAccessPoint) {
      networkTransition = NetworkTransition::AccessPointStarting;
      networkTransitionDueMs = millis() + 220;
    } else {
      networkTransition = NetworkTransition::None;
    }
    publishNetworkSnapshot();
    return;
  }
  if (networkTransition == NetworkTransition::StationStarting) {
    if (static_cast<int32_t>(now - networkTransitionDueMs) < 0) return;
    WiFi.mode(WIFI_STA);
    WiFi.setHostname("wio-memo");
    WiFi.begin(deviceSettings.stationSsid, deviceSettings.stationPassword);
    networkConnecting = true;
    networkAttemptStartedMs = millis();
    networkTransition = NetworkTransition::None;
    publishNetworkSnapshot();
    return;
  }
  if (networkTransition == NetworkTransition::AccessPointStarting) {
    if (static_cast<int32_t>(now - networkTransitionDueMs) < 0) return;
    WiFi.mode(WIFI_AP);
    const IPAddress portalAddress(192, 168, 5, 1);
    WiFi.softAPConfig(portalAddress, portalAddress, IPAddress(255, 255, 255, 0));
    WiFi.softAPsetHostname("wio-memo");
    apMode = WiFi.softAP(deviceSettings.accessPointSsid, deviceSettings.accessPointPassword);
    networkTransition = NetworkTransition::None;
    if (apMode) dnsServer.start(53, "*", portalAddress);
    if (apMode && webRoutesConfigured) server.begin();
    publishNetworkSnapshot();
    return;
  }
  if (!networkConnecting) return;
  if (WiFi.status() == WL_CONNECTED) {
    stationMode = true;
    apMode = false;
    networkConnecting = false;
    if (webRoutesConfigured) server.begin();
    publishNetworkSnapshot();
    ntpSyncPending = true;
    lastNtpAttemptMs = 0;
    Serial.print("Wio Memo LAN: http://");
    Serial.println(WiFi.localIP());
  } else if (millis() - networkAttemptStartedMs >= NETWORK_CONNECT_TIMEOUT_MS) {
    startAccessPoint();
  }
}

void stopNetwork() {
  if (webRoutesConfigured) server.stop();
  dnsServer.stop();
  stationMode = false;
  apMode = false;
  networkConnecting = false;
  networkTransition = NetworkTransition::StopForOffline;
  networkTransitionDueMs = millis();
  publishNetworkSnapshot();
}

void requestNetworkCommand(NetworkCommand command) {
  if (networkCommandQueue) {
    xQueueSend(networkCommandQueue, &command, 0);
    return;
  }
  if (command == NetworkCommand::Connect) connectNetwork();
  else if (command == NetworkCommand::AccessPoint) startAccessPoint();
  else stopNetwork();
}

bool getJsonFromIp(IPAddress address, const char *host, const String &path,
                   JsonDocument &document) {
  WiFiClient client;
  client.setTimeout(1);
  if (!client.connect(address, 80, 1000)) return false;
  client.print("GET ");
  client.print(path);
  client.print(" HTTP/1.0\r\nHost: ");
  client.print(host);
  client.print("\r\nAccept: application/json\r\nConnection: close\r\n\r\n");
  String response;
  response.reserve(1800);
  const uint32_t started = millis();
  while ((client.connected() || client.available()) && millis() - started < 3000) {
    while (client.available()) response += static_cast<char>(client.read());
    server.handleClient();
    checkReminders();
    buzzer.update(millis());
    lvglPort.update(millis());
    delay(1);
  }
  client.stop();
  const int statusEnd = response.indexOf("\r\n");
  const int bodyStart = response.indexOf("\r\n\r\n");
  if (statusEnd < 0 || bodyStart < 0 || response.indexOf(" 200 ", 0) < 0 ||
      response.indexOf(" 200 ", 0) > statusEnd) return false;
  return deserializeJson(document, response.c_str() + bodyStart + 4) ==
         DeserializationError::Ok;
}

bool fetchLocation() {
  JsonDocument location;
  const String path = "/json/?fields=status,message,city,lat,lon,offset&lang=zh-CN";
  if (!getJsonFromIp(IPAddress(208, 95, 112, 1), "ip-api.com", path, location) ||
      strcmp(location["status"] | "", "success") != 0)
    return false;
  const float latitude = location["lat"] | weatherLatitude;
  const float longitude = location["lon"] | weatherLongitude;
  if (latitude < -90.0F || latitude > 90.0F || longitude < -180.0F || longitude > 180.0F)
    return false;
  weatherLatitude = latitude;
  weatherLongitude = longitude;
  const char *city = location["city"] | "";
  if (city[0]) strlcpy(weatherCity, city, sizeof(weatherCity));
  const int offsetMinutes = (location["offset"] | 28800) / 60;
  if (offsetMinutes >= -720 && offsetMinutes <= 840 &&
      deviceSettings.utcOffsetMinutes != offsetMinutes) {
    wio_memo::DeviceSettings updated = deviceSettings;
    updated.utcOffsetMinutes = static_cast<int16_t>(offsetMinutes);
    if (persistentStore.saveSettings(taskList, updated)) deviceSettings = updated;
  }
  Serial.print("IP location: ");
  Serial.println(weatherCity);
  return true;
}

bool fetchForecast() {
  // Use Open-Meteo's IPv4 endpoint with an explicit Host header. rpcWiFi's DNS
  // resolver can block the cooperative UI/Web loop for several seconds.
  String path = "/v1/forecast?latitude=";
  path += String(weatherLatitude, 4);
  path += "&longitude=";
  path += String(weatherLongitude, 4);
  path += "&current=temperature_2m,relative_humidity_2m,weather_code,wind_speed_10m";
  path += "&daily=weather_code,temperature_2m_max,temperature_2m_min&forecast_days=4&timezone=auto";
  JsonDocument weather;
  if (!getJsonFromIp(IPAddress(188, 40, 99, 226), "api.open-meteo.com", path, weather) ||
      !weather["current"].is<JsonObject>())
    return false;
  JsonObject current = weather["current"].as<JsonObject>();
  cachedTemperature = static_cast<int16_t>(round(current["temperature_2m"].as<float>()));
  cachedHumidity = current["relative_humidity_2m"] | 0;
  cachedWeatherCode = current["weather_code"] | 3;
  cachedWindSpeed = static_cast<uint16_t>(round(current["wind_speed_10m"].as<float>()));
  const char *localIso = current["time"] | "";
  int year = 0, month = 0, day = 0, hour = 0, minute = 0;
  if (!clockValid &&
      sscanf(localIso, "%d-%d-%dT%d:%d", &year, &month, &day, &hour, &minute) == 5) {
    const uint32_t localEpoch = DateTime(year, month, day, hour, minute, 0).unixtime();
    const int32_t utcOffsetSeconds = weather["utc_offset_seconds"] | 0;
    const int64_t utcEpoch = static_cast<int64_t>(localEpoch) - utcOffsetSeconds;
    if (utcEpoch >= wio_memo::kMinimumValidEpoch && utcEpoch <= 0xFFFFFFFFLL) {
      rtc.adjust(DateTime(static_cast<uint32_t>(utcEpoch)));
      clockValid = true;
    }
  }
  deviceUi.setWeather(weatherCity, cachedTemperature, cachedHumidity, cachedAqi,
                      cachedWindSpeed, cachedWeatherCode, true);
  if (weather["daily"].is<JsonObject>()) {
    JsonObject daily = weather["daily"].as<JsonObject>();
    JsonArray dates = daily["time"].as<JsonArray>();
    JsonArray codes = daily["weather_code"].as<JsonArray>();
    JsonArray maxima = daily["temperature_2m_max"].as<JsonArray>();
    JsonArray minima = daily["temperature_2m_min"].as<JsonArray>();
    static const char *prefixes[3] = {"明天", "后天", "大后天"};
    for (uint8_t index = 0; index < 3; ++index) {
      const uint8_t source = index + 1;
      if (source >= dates.size()) break;
      const char *iso = dates[source] | "";
      char label[20]{};
      if (strlen(iso) >= 10) snprintf(label, sizeof(label), "%s %c%c/%c%c", prefixes[index],
                                      iso[5], iso[6], iso[8], iso[9]);
      else strlcpy(label, prefixes[index], sizeof(label));
      deviceUi.setForecast(index, label,
                           static_cast<int8_t>(round(minima[source].as<float>())),
                           static_cast<int8_t>(round(maxima[source].as<float>())),
                           codes[source] | 3);
    }
  }
  Serial.println("Local forecast updated");
  return true;
}

bool fetchAirQuality() {
  // Use the IPv4 endpoint with an explicit Host header, matching fetchForecast,
  // so rpcWiFi's blocking DNS resolver is never invoked on the cooperative loop.
  String path = "/v1/air-quality?latitude=";
  path += String(weatherLatitude, 4);
  path += "&longitude=";
  path += String(weatherLongitude, 4);
  path += "&current=us_aqi&timezone=auto";
  JsonDocument air;
  if (!getJsonFromIp(IPAddress(152, 53, 84, 73), "air-quality-api.open-meteo.com", path, air) ||
      !air["current"].is<JsonObject>())
    return false;
  cachedAqi = static_cast<uint16_t>(round(air["current"]["us_aqi"].as<float>()));
  deviceUi.setWeather(weatherCity, cachedTemperature, cachedHumidity, cachedAqi,
                      cachedWindSpeed, cachedWeatherCode, true);
  return true;
}

void startWeatherFetch(uint32_t now) {
  weatherFetchPending = true;
  weatherFetchStage = WeatherFetchStage::Location;
  weatherStageDueMs = now + NETWORK_STAGE_GAP_MS;
}

void serviceWeatherFetch(uint32_t now) {
  if (!stationMode || !weatherFetchPending ||
      static_cast<int32_t>(now - weatherStageDueMs) < 0) return;
  if (weatherFetchStage == WeatherFetchStage::Location) {
    // IP geolocation is best-effort. A failure keeps the last known coordinates.
    fetchLocation();
    weatherFetchStage = WeatherFetchStage::Forecast;
    weatherStageDueMs = millis() + NETWORK_STAGE_GAP_MS;
  } else if (weatherFetchStage == WeatherFetchStage::Forecast) {
    if (fetchForecast()) {
      // Chain into the air-quality stage instead of finishing, so AQI is
      // actually fetched. A failure there is non-fatal and still ends the cycle.
      weatherFetchStage = WeatherFetchStage::AirQuality;
      weatherStageDueMs = millis() + NETWORK_STAGE_GAP_MS;
    } else {
      weatherFetchStage = WeatherFetchStage::RetryWait;
      weatherStageDueMs = millis() + WEATHER_RETRY_MS;
    }
  } else if (weatherFetchStage == WeatherFetchStage::AirQuality) {
    fetchAirQuality();
    weatherFetchPending = false;
    weatherFetchStage = WeatherFetchStage::Idle;
    lastWeatherFetchMs = millis();
  } else if (weatherFetchStage == WeatherFetchStage::RetryWait) {
    startWeatherFetch(millis());
  }
}

void handleUiAction() {
  const wio_memo::UiAction action = deviceUi.takeAction();
  if (action == wio_memo::UiAction::None) return;
  switch (action) {
    case wio_memo::UiAction::NetworkStation:
      requestNetworkCommand(NetworkCommand::Connect);
      break;
    case wio_memo::UiAction::NetworkAccessPoint:
      requestNetworkCommand(NetworkCommand::AccessPoint);
      break;
    case wio_memo::UiAction::NetworkOffline:
      requestNetworkCommand(NetworkCommand::Offline);
      break;
    case wio_memo::UiAction::ToggleMute:
      muted = !muted;
      buzzer.setMuted(muted);
      deviceUi.show(wio_memo::UiPage::Settings);
      break;
    case wio_memo::UiAction::TestSound:
      buzzer.play(wio_memo::AlertType::Start, millis());
      break;
    default:
      break;
  }
}

const char *networkLabel() {
  if (networkTransition != NetworkTransition::None) return "连接中";
  if (stationMode) return "局域网";
  if (apMode) return "设备热点";
  if (networkConnecting) return "连接中";
  return "离线";
}

String networkIp() {
  if (stationMode) return WiFi.localIP().toString();
  if (apMode) return WiFi.softAPIP().toString();
  return "--";
}

String networkDetails() {
  if (networkTransition == NetworkTransition::StopForStation ||
      networkTransition == NetworkTransition::StationStarting)
    return "正在切换到局域网\nWi-Fi  " + String(deviceSettings.stationSsid) + "\n请稍候";
  if (networkTransition == NetworkTransition::StopForAccessPoint ||
      networkTransition == NetworkTransition::AccessPointStarting)
    return "正在开启设备热点\n请稍候\n完成后访问 http://192.168.5.1";
  if (networkTransition == NetworkTransition::StopForOffline)
    return "正在关闭网络\n离线功能仍可使用";
  if (apMode) {
    return "AP  " + String(deviceSettings.accessPointSsid) + "\n密码  " +
           String(deviceSettings.accessPointPassword) + "\n网址  http://" + networkIp() + "/";
  }
  if (stationMode) {
    const String ip = networkIp();
    return "Wi-Fi  " + WiFi.SSID() + "\n设备 IP  " + ip + "\n网页  http://" + ip + "/";
  }
  if (networkConnecting) return "正在连接\nWi-Fi  " + String(deviceSettings.stationSsid) +
                                "\n请稍候";
  return "离线模式\n待办和提醒仍可使用\n网络服务已关闭";
}

void publishNetworkSnapshot() {
  NetworkSnapshot next{};
  const String ip = networkIp();
  const String details = networkDetails();
  strlcpy(next.label, networkLabel(), sizeof(next.label));
  strlcpy(next.ip, ip.c_str(), sizeof(next.ip));
  strlcpy(next.details, details.c_str(), sizeof(next.details));
  static char loggedLabel[16]{};
  static char loggedIp[20]{};
  if (strcmp(loggedLabel, next.label) != 0 || strcmp(loggedIp, next.ip) != 0) {
    strlcpy(loggedLabel, next.label, sizeof(loggedLabel));
    strlcpy(loggedIp, next.ip, sizeof(loggedIp));
    Serial.print("Network state: ");
    Serial.print(next.label);
    Serial.print(" | IP: ");
    Serial.print(next.ip);
    Serial.print(" | WiFi status: ");
    Serial.println(static_cast<int>(WiFi.status()));
  }
  if (!networkSnapshotMutex ||
      xSemaphoreTake(networkSnapshotMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
    networkSnapshot = next;
    if (networkSnapshotMutex) xSemaphoreGive(networkSnapshotMutex);
  }
}

NetworkSnapshot readNetworkSnapshot() {
  NetworkSnapshot copy{};
  if (networkSnapshotMutex &&
      xSemaphoreTake(networkSnapshotMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
    copy = networkSnapshot;
    xSemaphoreGive(networkSnapshotMutex);
  } else {
    copy = networkSnapshot;
  }
  return copy;
}

String twoDigits(int value) {
  return value < 10 ? "0" + String(value) : String(value);
}

String formatTime(uint32_t utc, bool includeDate) {
  if (!utc) return "--:--";
  DateTime local(wio_memo::ClockPolicy::toLocalEpoch(utc, deviceSettings.utcOffsetMinutes));
  String result;
  if (includeDate) result = twoDigits(local.month()) + "/" + twoDigits(local.day()) + " ";
  return result + twoDigits(local.hour()) + ":" + twoDigits(local.minute());
}

String formatClockTime(uint32_t utc) {
  if (!utc) return "--:--:--";
  DateTime local(wio_memo::ClockPolicy::toLocalEpoch(utc, deviceSettings.utcOffsetMinutes));
  return twoDigits(local.hour()) + ":" + twoDigits(local.minute()) + ":" +
         twoDigits(local.second());
}

String formatDashboardDate(uint32_t utc) {
  if (!utc) return "等待对时";
  DateTime local(wio_memo::ClockPolicy::toLocalEpoch(utc, deviceSettings.utcOffsetMinutes));
  static const char *weekdays[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  return String(local.year()) + "/" + twoDigits(local.month()) + "/" +
         twoDigits(local.day()) + "  " + weekdays[local.dayOfTheWeek()];
}

void checkReminders() {
  const uint32_t now = nowUtc();
  if (!now) return;
  const wio_memo::ReminderEvent event = reminderEngine.poll(now, taskList);
  if (event.stateChanged) persistentStore.save(taskList);
  if (event.type != wio_memo::AlertType::None) buzzer.play(event.type, millis());
}

void setupPins() {
  buzzer.begin();
  pinMode(WIO_5S_UP, INPUT_PULLUP);
  pinMode(WIO_5S_DOWN, INPUT_PULLUP);
  pinMode(WIO_5S_LEFT, INPUT_PULLUP);
  pinMode(WIO_5S_RIGHT, INPUT_PULLUP);
  pinMode(WIO_5S_PRESS, INPUT_PULLUP);
  pinMode(WIO_KEY_A, INPUT_PULLUP);
  pinMode(WIO_KEY_B, INPUT_PULLUP);
  pinMode(WIO_KEY_C, INPUT_PULLUP);
}

void runUiCycle() {
    const bool hardWake = digitalRead(WIO_KEY_A) == LOW || digitalRead(WIO_KEY_B) == LOW ||
                          digitalRead(WIO_KEY_C) == LOW || digitalRead(WIO_5S_PRESS) == LOW ||
                          digitalRead(WIO_5S_UP) == LOW || digitalRead(WIO_5S_DOWN) == LOW ||
                          digitalRead(WIO_5S_LEFT) == LOW || digitalRead(WIO_5S_RIGHT) == LOW;
    if (hardWake && deviceUi.page() == wio_memo::UiPage::Standby) {
      deviceUi.show(wio_memo::UiPage::Home);
    }
    if (deviceUi.pollShortcuts(muted)) {
      muted = !muted;
      buzzer.setMuted(muted);
    }
    const wio_memo::UiPage activePage = deviceUi.page();
    lvglPort.setKeypadEnabled(activePage != wio_memo::UiPage::Menu &&
                              activePage != wio_memo::UiPage::GameBreakout &&
                              activePage != wio_memo::UiPage::GameTetris);
    handleUiAction();
    checkReminders();
    buzzer.update(millis());
    if (millis() - lastDisplayMs >= UI_STATUS_REFRESH_MS) {
      lastDisplayMs = millis();
      const String time = formatTime(nowUtc());
      const String clockTime = formatClockTime(nowUtc());
      const String date = formatDashboardDate(nowUtc());
      const NetworkSnapshot snapshot = readNetworkSnapshot();
      deviceUi.update(time.c_str(), snapshot.label, snapshot.ip, snapshot.details, date.c_str(),
                      clockTime.c_str());
    }
    lvglPort.update(millis());
}

void uiTask(void *) {
  for (;;) {
    runUiCycle();
    vTaskDelay(pdMS_TO_TICKS(2));
  }
}

void networkTask(void *) {
  ntpUdp.begin(NTP_LOCAL_PORT);
  if (accessPointBootRequested) startAccessPoint();
  else connectNetwork();
  uint32_t lastSnapshotMs = 0;
  for (;;) {
    NetworkCommand command;
    if (xQueueReceive(networkCommandQueue, &command, 0) == pdTRUE) {
      if (command == NetworkCommand::Connect) connectNetwork();
      else if (command == NetworkCommand::AccessPoint) startAccessPoint();
      else stopNetwork();
    }
    if (apMode) dnsServer.processNextRequest();
    if (webRoutesConfigured && (stationMode || apMode)) server.handleClient();
    updateNetwork();
    if (Serial.available()) {
      const char commandChar = static_cast<char>(Serial.read());
      if (commandChar == 'a' || commandChar == 'A') startAccessPoint();
      if (commandChar == 's' || commandChar == 'S') connectNetwork();
    }
    if (millis() - lastNetworkStatusLogMs >= 5000) {
      lastNetworkStatusLogMs = millis();
      if (stationMode) {
        Serial.print("Wio Memo LAN: http://");
        Serial.println(WiFi.localIP());
      } else if (apMode) {
        Serial.print("Wio Memo AP: http://");
        Serial.println(WiFi.softAPIP());
      }
    }
    serviceNtp(millis());
    if (millis() - lastSnapshotMs >= 250) {
      lastSnapshotMs = millis();
      publishNetworkSnapshot();
    }
    vTaskDelay(pdMS_TO_TICKS(2));
  }
}

}  // namespace

// Seeed's SAMD51 FreeRTOS port shares Arduino's SysTick interrupt. Without
// forwarding the tick to the kernel, every task blocks forever after its first
// vTaskDelay(), leaving the LCD on its last rendered frame.
extern void xPortSysTickHandler(void);
int sysTickHook(void) {
  if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
    xPortSysTickHandler();
  }
  return 0;
}

void setup() {
  Serial.begin(115200);
  setupPins();
  persistentStore.begin(taskList, deviceSettings);
  accessPointBootRequested = deviceSettings.reserved == START_ACCESS_POINT_ONCE;
  if (accessPointBootRequested) {
    wio_memo::DeviceSettings updated = deviceSettings;
    updated.reserved = BUNDLED_NETWORK_APPLIED;
    if (persistentStore.saveSettings(taskList, updated)) deviceSettings = updated;
  }
  applyBundledNetworkDefaults();
  rtc.begin();
  clockValid = rtc.now().unixtime() >= wio_memo::kMinimumValidEpoch;
  tft.begin();
  tft.setRotation(3);
  const bool font16Ready = qspiFont16.begin(wio_memo::kQspiFont16Address);
  const bool font12Ready = qspiFont12.begin(wio_memo::kQspiFont12Address);
  const bool font20Ready = qspiFont20.begin(wio_memo::kQspiFont20Address);
  lvglPort.begin(tft);
  qspiLvglFont16.begin(qspiFont16, &lv_font_montserrat_16);
  qspiLvglFont12.begin(qspiFont12, &lv_font_montserrat_12);
  qspiLvglFont20.begin(qspiFont20, &lv_font_montserrat_20);
  deviceUi.begin(font16Ready ? qspiLvglFont16.font() : &lv_font_montserrat_16,
                 font20Ready ? qspiLvglFont20.font() : &lv_font_montserrat_20,
                 font12Ready ? qspiLvglFont12.font() : &lv_font_montserrat_12);
  lvglPort.update(millis());
  Serial.println(font16Ready && font12Ready && font20Ready
                     ? "QSPI fonts 12/16/20 ready"
                     : "QSPI font set incomplete; using partial fallback");
  setupWebServer();
  if (SAFE_UI_ONLY_MODE) {
    strlcpy(networkSnapshot.label, "离线", sizeof(networkSnapshot.label));
    strlcpy(networkSnapshot.ip, "--", sizeof(networkSnapshot.ip));
    strlcpy(networkSnapshot.details,
            "天气界面已就绪\n正在准备连接已保存的 Wi-Fi",
            sizeof(networkSnapshot.details));
    Serial.println("Wio Memo cooperative weather mode");
    return;
  }
  networkCommandQueue = xQueueCreate(4, sizeof(NetworkCommand));
  networkSnapshotMutex = xSemaphoreCreateMutex();
  publishNetworkSnapshot();
  if (!networkCommandQueue || !networkSnapshotMutex ||
      xTaskCreate(uiTask, "wio-ui", 4096, nullptr, 2, nullptr) != pdPASS ||
      xTaskCreate(networkTask, "wio-net", 3072, nullptr, 1, nullptr) != pdPASS) {
    Serial.println("FreeRTOS task creation failed");
    while (true) delay(1000);
  }
#if defined(USE_TINYUSB)
  extern void tinyusb_task(void);
  tinyusb_task();
#endif
  vTaskStartScheduler();
  while (true) {}
}

void loop() {
  if (SAFE_UI_ONLY_MODE) {
    runUiCycle();
    const uint32_t now = millis();
    if (!cooperativeNetworkStarted && now >= 3200) {
      cooperativeNetworkStarted = true;
      ntpUdp.begin(NTP_LOCAL_PORT);
      if (accessPointBootRequested) startAccessPoint();
      else connectNetwork();
    }
    if (cooperativeNetworkStarted) {
      if (apMode) dnsServer.processNextRequest();
      if (webRoutesConfigured && (stationMode || apMode)) server.handleClient();
      if (Serial.available()) {
        const char commandChar = static_cast<char>(Serial.read());
        if (commandChar == 'a' || commandChar == 'A') startAccessPoint();
        if (commandChar == 's' || commandChar == 'S') connectNetwork();
      }
      const bool wasStation = stationMode;
      updateNetwork();
      if (!wasStation && stationMode) startWeatherFetch(millis());
      serviceNtp(millis());
      if (stationMode && !weatherFetchPending &&
          now - lastWeatherFetchMs >= WEATHER_REFRESH_MS) {
        startWeatherFetch(now);
      }
      serviceWeatherFetch(millis());
      if (now - lastCooperativeSnapshotMs >= 500) {
        lastCooperativeSnapshotMs = now;
        publishNetworkSnapshot();
      }
      if (now - lastNetworkStatusLogMs >= 5000) {
        lastNetworkStatusLogMs = now;
        Serial.print("Network heartbeat: ");
        Serial.print(networkLabel());
        Serial.print(" | IP: ");
        Serial.print(networkIp());
        Serial.print(" | WiFi status: ");
        Serial.println(static_cast<int>(WiFi.status()));
      }
    }
    delay(2);
  }
}
