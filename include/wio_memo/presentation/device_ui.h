#pragma once

#include <lvgl.h>

#include "wio_memo/application/task_service.h"
#include "wio_memo/infrastructure/persistent_store.h"

namespace wio_memo {

enum class WeatherKind : uint8_t {
  Clear, PartlyCloudy, Cloudy, Rain, Thunder, Snow, Fog
};

enum class UiPage : uint8_t {
  Splash, Home, Menu, Clock, Tasks, Network, Games, GameLights, GameMemory, GameBreakout,
  GameTetris,
  Settings, Standby
};
enum class UiAction : uint8_t {
  None,
  NetworkStation,
  NetworkAccessPoint,
  NetworkOffline,
  ToggleMute,
  TestSound
};

class DeviceUi {
 public:
  DeviceUi(TaskList &tasks, TaskService &taskService, PersistentStore &store)
      : tasks_(tasks), taskService_(taskService), store_(store) {}

  void begin(const lv_font_t *bodyFont, const lv_font_t *titleFont, const lv_font_t *smallFont);
  void show(UiPage page);
  void update(const char *timeText, const char *networkText, const char *ipText,
              const char *networkDetails, const char *dateText = "");
  bool pollShortcuts(bool muted);
  UiAction takeAction();
  UiPage page() const { return page_; }
  void setWeather(const char *city, int16_t temperature, uint8_t humidity, uint16_t aqi,
                  uint16_t windSpeed, uint8_t weatherCode, bool live);
  void setForecast(uint8_t index, const char *date, int8_t minimum, int8_t maximum,
                   uint8_t weatherCode);

 private:
  static void menuEvent(lv_event_t *event);
  static void actionEvent(lv_event_t *event);
  static void confirmActionEvent(lv_event_t *event);
  static void cancelActionEvent(lv_event_t *event);
  static void gameCellEvent(lv_event_t *event);
  static void gameResetEvent(lv_event_t *event);
  static void memoryCellEvent(lv_event_t *event);
  static void memoryResetEvent(lv_event_t *event);
  static void completeNextEvent(lv_event_t *event);
  static lv_obj_t *makeButton(lv_obj_t *parent, const char *text, int16_t x, int16_t y,
                              int16_t width, int16_t height, lv_event_cb_t callback,
                              void *userData);
  lv_obj_t *makeMenuCard(lv_obj_t *parent, const char *badge, const char *title,
                         const char *subtitle, int16_t x, int16_t y, UiPage page);
  lv_obj_t *makeActionButton(lv_obj_t *parent, const char *text, int16_t x, int16_t y,
                             int16_t width, int16_t height, UiAction action);
  void buildSplash();
  void buildStandby();
  void buildClock(bool standby);
  void buildHome();
  void buildMenu();
  void buildDetail(UiPage page);
  void refreshHome();
  void refreshTasks();
  void resetGame();
  void refreshGame();
  void resetMemoryGame();
  void refreshMemoryGame();
  void resetBreakout();
  void refreshBreakout();
  void updateBreakout(uint32_t now);
  void resetTetris();
  void refreshTetris();
  void updateTetris(uint32_t now);
  bool tetrisCanPlace(int8_t x, int8_t y, uint8_t rotation) const;
  void lockTetrisPiece();
  void spawnTetrisPiece();
  void refreshClock(const char *timeText);
  void refreshStandbyClock(const char *timeText);
  void refreshNetworkBadge(const char *networkText);
  void refreshMenuFocus();
  void rebuildWeatherScene();
  void rebuildForecastIcon(uint8_t index);
  void showWeatherPreview(int8_t direction);
  void setScreenBase(lv_obj_t *screen);
  void addHeader(lv_obj_t *screen, const char *title);
  void addTip(lv_obj_t *screen, const char *text);
  void showActionDialog(UiAction action);
  void closeActionDialog();

  TaskList &tasks_;
  TaskService &taskService_;
  PersistentStore &store_;
  const lv_font_t *font_ = nullptr;
  const lv_font_t *titleFont_ = nullptr;
  const lv_font_t *smallFont_ = nullptr;
  UiPage page_ = UiPage::Splash;
  lv_obj_t *screen_ = nullptr;
  lv_obj_t *mascotImage_ = nullptr;
  lv_obj_t *timeLabel_ = nullptr;
  lv_obj_t *networkLabel_ = nullptr;
  lv_obj_t *networkStatusDot_ = nullptr;
  lv_obj_t *taskTitleLabel_ = nullptr;
  lv_obj_t *taskTimeLabel_ = nullptr;
  lv_obj_t *weatherScene_ = nullptr;
  lv_obj_t *weatherCityLabel_ = nullptr;
  lv_obj_t *weatherTemperatureLabel_ = nullptr;
  lv_obj_t *weatherConditionLabel_ = nullptr;
  lv_obj_t *weatherHumidityLabel_ = nullptr;
  lv_obj_t *weatherAqiLabel_ = nullptr;
  lv_obj_t *weatherWindLabel_ = nullptr;
  lv_obj_t *weatherSourceLabel_ = nullptr;
  lv_obj_t *weatherDateLabel_ = nullptr;
  lv_obj_t *forecastIcon_[3]{};
  lv_obj_t *forecastDateLabel_[3]{};
  lv_obj_t *forecastTemperatureLabel_[3]{};
  lv_obj_t *taskListLabel_ = nullptr;
  lv_obj_t *detailLabel_ = nullptr;
  lv_obj_t *actionDialog_ = nullptr;
  lv_obj_t *gameCells_[9]{};
  lv_obj_t *gameStatusLabel_ = nullptr;
  lv_obj_t *menuCards_[6]{};
  lv_obj_t *menuHintLabel_ = nullptr;
  lv_obj_t *clockDigits_[4]{};
  lv_obj_t *clockSegments_[4][7]{};
  lv_obj_t *clockColon_[2]{};
  lv_obj_t *clockSyncLabel_ = nullptr;
  lv_obj_t *standbyDigitCards_[4]{};
  lv_obj_t *standbyDigitLabels_[4]{};
  lv_obj_t *standbyColon_ = nullptr;
  lv_obj_t *memoryCells_[8]{};
  lv_obj_t *memoryLabels_[8]{};
  lv_obj_t *breakoutBoard_ = nullptr;
  lv_obj_t *breakoutPaddle_ = nullptr;
  lv_obj_t *breakoutBall_ = nullptr;
  lv_obj_t *breakoutBricks_[18]{};
  lv_obj_t *tetrisBoard_ = nullptr;
  char networkSummary_[160]{};
  bool previousA_ = false;
  bool previousB_ = false;
  bool previousC_ = false;
  bool previousUp_ = false;
  bool previousDown_ = false;
  bool previousLeft_ = false;
  bool previousRight_ = false;
  bool previousPress_ = false;
  uint8_t menuFocus_ = 0;
  bool muted_ = false;
  bool networkBusy_ = false;
  UiAction pendingAction_ = UiAction::None;
  UiAction confirmAction_ = UiAction::None;
  uint32_t pageStartedMs_ = 0;
  uint32_t pageTransitionUntilMs_ = 0;
  uint32_t lastInteractionMs_ = 0;
  int16_t mascotBaseY_ = 0;
  uint16_t gameLights_ = 0x111;
  uint8_t gameMoves_ = 0;
  char clockValues_[4] = {'?', '?', '?', '?'};
  char standbyClockValues_[4] = {'?', '?', '?', '?'};
  uint8_t memoryValues_[8] = {1, 2, 3, 4, 1, 2, 3, 4};
  uint8_t memoryMatched_ = 0;
  uint8_t memoryVisible_ = 0;
  int8_t memoryFirst_ = -1;
  uint32_t memoryHideAtMs_ = 0;
  uint32_t breakoutBricksAlive_ = 0;
  int16_t breakoutPaddleX_ = 74;
  int16_t breakoutBallX_ = 99;
  int16_t breakoutBallY_ = 110;
  int8_t breakoutBallDx_ = 2;
  int8_t breakoutBallDy_ = -2;
  uint16_t breakoutScore_ = 0;
  uint32_t breakoutLastTickMs_ = 0;
  bool breakoutRunning_ = false;
  uint8_t tetrisRows_[12]{};
  uint8_t tetrisShape_ = 0;
  uint8_t tetrisRotation_ = 0;
  int8_t tetrisX_ = 2;
  int8_t tetrisY_ = 0;
  uint16_t tetrisScore_ = 0;
  uint32_t tetrisLastTickMs_ = 0;
  bool tetrisGameOver_ = false;
  WeatherKind weatherKind_ = WeatherKind::Clear;
  uint8_t weatherPreviewIndex_ = 0;
  int16_t weatherTemperature_ = 28;
  uint8_t weatherHumidity_ = 65;
  uint16_t weatherAqi_ = 32;
  uint16_t weatherWindSpeed_ = 9;
  uint8_t weatherCode_ = 0;
  bool weatherLive_ = false;
  char weatherCity_[32] = "深圳";
  char forecastDate_[3][20] = {"明天", "后天", "周末"};
  int8_t forecastMinimum_[3] = {20, 21, 22};
  int8_t forecastMaximum_[3] = {26, 27, 28};
  uint8_t forecastCode_[3] = {2, 2, 1};
};

}  // namespace wio_memo
