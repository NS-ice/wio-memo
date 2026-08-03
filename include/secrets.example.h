#pragma once

// Copy this file to include/secrets.h and fill in your Wi-Fi credentials.
// If it is absent or SSID is empty, the device starts its own access point:
//   SSID: WioMemo-Setup   password: wio-memo
#define WIFI_SSID "your-wifi-name"
#define WIFI_PASSWORD "your-wifi-password"

// Local time offset from UTC, in minutes. China Standard Time = UTC+8 = 480.
#define LOCAL_UTC_OFFSET_MINUTES 480
#define WIFI_FORCE_PROVISION 0
