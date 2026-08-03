#include "wio_memo/presentation/device_ui.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

#include "wio_memo/presentation/memo_mascot_assets.h"

namespace wio_memo {
namespace {

constexpr lv_color_t kBackground = LV_COLOR_MAKE(0xF7, 0xF3, 0xEA);
constexpr lv_color_t kInk = LV_COLOR_MAKE(0x20, 0x36, 0x3D);
constexpr lv_color_t kMint = LV_COLOR_MAKE(0x7A, 0xD7, 0xC4);
constexpr lv_color_t kMintSoft = LV_COLOR_MAKE(0xE2, 0xF4, 0xEF);
constexpr lv_color_t kMintDark = LV_COLOR_MAKE(0x2D, 0x79, 0x73);
constexpr lv_color_t kCream = LV_COLOR_MAKE(0xFF, 0xFC, 0xF4);
constexpr lv_color_t kCoral = LV_COLOR_MAKE(0xFF, 0x82, 0x72);
constexpr lv_color_t kMutedInk = LV_COLOR_MAKE(0x68, 0x7B, 0x80);
constexpr lv_color_t kClockOff = LV_COLOR_MAKE(0x2A, 0x45, 0x49);
constexpr uint32_t kSplashDurationMs = 2600;
constexpr uint32_t kStandbyDelayMs = 60000;
constexpr bool kAutomaticStandbyEnabled = false;

const char *pageTitle(UiPage page) {
  switch (page) {
    case UiPage::Clock: return "翻页时钟";
    case UiPage::Tasks: return "待办与会议";
    case UiPage::Network: return "网络模式";
    case UiPage::Games: return "益智小游戏";
    case UiPage::GameLights: return "点灯谜题";
    case UiPage::GameMemory: return "记忆翻牌";
    case UiPage::GameBreakout: return "复古打砖块";
    case UiPage::GameTetris: return "俄罗斯方块";
    case UiPage::Settings: return "设备设置";
    default: return "菜单";
  }
}

WeatherKind weatherKindFromCode(uint8_t code) {
  if (code == 0) return WeatherKind::Clear;
  if (code <= 2) return WeatherKind::PartlyCloudy;
  if (code == 3) return WeatherKind::Cloudy;
  if (code == 45 || code == 48) return WeatherKind::Fog;
  if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) return WeatherKind::Rain;
  if ((code >= 71 && code <= 77) || (code >= 85 && code <= 86)) return WeatherKind::Snow;
  if (code >= 95) return WeatherKind::Thunder;
  return WeatherKind::Cloudy;
}

const char *weatherName(WeatherKind kind) {
  switch (kind) {
    case WeatherKind::Clear: return "晴朗";
    case WeatherKind::PartlyCloudy: return "晴间多云";
    case WeatherKind::Cloudy: return "多云";
    case WeatherKind::Rain: return "有雨";
    case WeatherKind::Thunder: return "雷阵雨";
    case WeatherKind::Snow: return "降雪";
    case WeatherKind::Fog: return "有雾";
  }
  return "天气";
}

// Five compact 4x4 tetrominoes. Bit (row * 4 + column) describes one block.
constexpr uint16_t kTetrisShapes[5][4] = {
    {0x00F0, 0x2222, 0x00F0, 0x2222},  // I
    {0x0066, 0x0066, 0x0066, 0x0066},  // O
    {0x0072, 0x0262, 0x0270, 0x0232},  // T
    {0x0071, 0x0226, 0x0470, 0x0322},  // L
    {0x0036, 0x0462, 0x0036, 0x0462},  // S
};

}  // namespace

void DeviceUi::buildSplash() {
  screen_ = lv_obj_create(nullptr);
  setScreenBase(screen_);

  lv_obj_t *title = lv_label_create(screen_);
  lv_label_set_text(title, "WIO MEMO");
  lv_obj_set_style_text_font(title, titleFont_, 0);
  lv_obj_set_style_text_color(title, kMintDark, 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 9);

  mascotImage_ = lv_img_create(screen_);
  lv_img_set_src(mascotImage_, &memo_mascot_large);
  mascotBaseY_ = 31;
  lv_obj_set_pos(mascotImage_, 91, mascotBaseY_);

  lv_obj_t *subtitle = lv_label_create(screen_);
  lv_label_set_text(subtitle, "今天也要轻松一点");
  lv_obj_set_style_text_color(subtitle, kMintDark, 0);
  lv_obj_align(subtitle, LV_ALIGN_BOTTOM_MID, 0, -12);
}

void DeviceUi::buildStandby() {
  screen_ = lv_obj_create(nullptr);
  setScreenBase(screen_);
  for (char &value : standbyClockValues_) value = '?';

  const int16_t xs[4] = {95, 124, 171, 200};
  for (uint8_t digit = 0; digit < 4; ++digit) {
    lv_obj_t *card = lv_obj_create(screen_);
    standbyDigitCards_[digit] = card;
    lv_obj_set_pos(card, xs[digit], 5);
    lv_obj_set_size(card, 26, 32);
    lv_obj_set_style_radius(card, 7, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, LV_COLOR_MAKE(0xC8, 0xDE, 0xEE), 0);
    lv_obj_set_style_bg_color(card, LV_COLOR_MAKE(0x4F, 0x83, 0xA8), 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *label = lv_label_create(card);
    standbyDigitLabels_[digit] = label;
    lv_obj_set_style_text_font(label, titleFont_, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_center(label);
    lv_obj_t *split = lv_obj_create(card);
    lv_obj_set_pos(split, 1, 15);
    lv_obj_set_size(split, 24, 1);
    lv_obj_set_style_border_width(split, 0, 0);
    lv_obj_set_style_bg_color(split, LV_COLOR_MAKE(0x35, 0x67, 0x8B), 0);
    lv_obj_clear_flag(split, LV_OBJ_FLAG_SCROLLABLE);
  }
  standbyColon_ = lv_label_create(screen_);
  lv_label_set_text(standbyColon_, ":");
  lv_obj_set_style_text_font(standbyColon_, titleFont_, 0);
  lv_obj_set_style_text_color(standbyColon_, LV_COLOR_MAKE(0x4F, 0x83, 0xA8), 0);
  lv_obj_set_pos(standbyColon_, 155, 9);

  mascotImage_ = lv_img_create(screen_);
  lv_img_set_src(mascotImage_, &memo_mascot_large);
  mascotBaseY_ = 42;
  lv_obj_set_pos(mascotImage_, 91, mascotBaseY_);
  addTip(screen_, "小贴士：轻按任意键即可唤醒");
}

void DeviceUi::refreshStandbyClock(const char *timeText) {
  if (!standbyDigitCards_[0]) return;
  const bool valid = timeText && strlen(timeText) >= 5 && timeText[2] == ':';
  const char values[4] = {valid ? timeText[0] : '-', valid ? timeText[1] : '-',
                          valid ? timeText[3] : '-', valid ? timeText[4] : '-'};
  for (uint8_t digit = 0; digit < 4; ++digit) {
    if (standbyClockValues_[digit] == values[digit]) continue;
    standbyClockValues_[digit] = values[digit];
    char text[2] = {values[digit], '\0'};
    lv_label_set_text(standbyDigitLabels_[digit], text);
    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, standbyDigitCards_[digit]);
    lv_anim_set_values(&animation, 1, 5);
    lv_anim_set_time(&animation, 170);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&animation, [](void *object, int32_t y) {
      lv_obj_set_y(static_cast<lv_obj_t *>(object), y);
    });
    lv_anim_start(&animation);
  }
  if (standbyColon_) {
    lv_obj_set_style_text_opa(standbyColon_, ((millis() / 500U) & 1U) ? LV_OPA_40 :
                                                                LV_OPA_COVER, 0);
  }
}

void DeviceUi::buildClock(bool standby) {
  screen_ = lv_obj_create(nullptr);
  setScreenBase(screen_);
  lv_obj_set_style_bg_color(screen_, kInk, 0);
  for (char &value : clockValues_) value = '?';

  lv_obj_t *brand = lv_label_create(screen_);
  lv_label_set_text(brand, standby ? "WIO MEMO - STANDBY" : "WIO MEMO - CLOCK");
  lv_obj_set_style_text_font(brand, smallFont_, 0);
  lv_obj_set_style_text_color(brand, kMint, 0);
  lv_obj_align(brand, LV_ALIGN_TOP_MID, 0, 8);

  const int16_t xs[4] = {22, 84, 174, 236};
  for (uint8_t digit = 0; digit < 4; ++digit) {
    lv_obj_t *card = lv_obj_create(screen_);
    clockDigits_[digit] = card;
    lv_obj_set_pos(card, xs[digit], 40);
    lv_obj_set_size(card, 60, 100);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, LV_COLOR_MAKE(0x3D, 0x59, 0x5D), 0);
    lv_obj_set_style_bg_color(card, LV_COLOR_MAKE(0x19, 0x2D, 0x33), 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    const int16_t sx[7] = {10, 47, 47, 10, 5, 5, 10};
    const int16_t sy[7] = {8, 13, 54, 87, 54, 13, 47};
    const int16_t sw[7] = {40, 7, 7, 40, 7, 7, 40};
    const int16_t sh[7] = {7, 35, 35, 7, 35, 35, 7};
    for (uint8_t segment = 0; segment < 7; ++segment) {
      lv_obj_t *bar = lv_obj_create(card);
      clockSegments_[digit][segment] = bar;
      lv_obj_set_pos(bar, sx[segment], sy[segment]);
      lv_obj_set_size(bar, sw[segment], sh[segment]);
      lv_obj_set_style_radius(bar, 4, 0);
      lv_obj_set_style_border_width(bar, 0, 0);
      lv_obj_set_style_bg_color(bar, LV_COLOR_MAKE(0x2A, 0x45, 0x49), 0);
      lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    }
    lv_obj_t *split = lv_obj_create(card);
    lv_obj_set_pos(split, 2, 49);
    lv_obj_set_size(split, 56, 2);
    lv_obj_set_style_border_width(split, 0, 0);
    lv_obj_set_style_bg_color(split, LV_COLOR_MAKE(0x0F, 0x22, 0x27), 0);
    lv_obj_clear_flag(split, LV_OBJ_FLAG_SCROLLABLE);
  }
  uint8_t colonIndex = 0;
  const int16_t colonY[2] = {68, 105};
  for (int16_t y : colonY) {
    lv_obj_t *dot = lv_obj_create(screen_);
    clockColon_[colonIndex++] = dot;
    lv_obj_set_pos(dot, 157, y);
    lv_obj_set_size(dot, 8, 8);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(dot, 0, 0);
    lv_obj_set_style_bg_color(dot, kCoral, 0);
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
  }
  clockSyncLabel_ = lv_label_create(screen_);
  lv_obj_set_style_text_font(clockSyncLabel_, smallFont_, 0);
  lv_obj_set_style_text_color(clockSyncLabel_, kMint, 0);
  lv_obj_align(clockSyncLabel_, LV_ALIGN_BOTTOM_MID, 0, -43);
  addTip(screen_, standby ? "小贴士：轻按任意键即可唤醒" :
                            "小贴士：联网后会自动校准时间");
}

void DeviceUi::refreshClock(const char *timeText) {
  if (!clockDigits_[0]) return;
  const bool valid = timeText && strlen(timeText) >= 5 && timeText[2] == ':';
  const char values[4] = {valid ? timeText[0] : '-', valid ? timeText[1] : '-',
                          valid ? timeText[3] : '-', valid ? timeText[4] : '-'};
  static constexpr uint8_t masks[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66,
                                         0x6D, 0x7D, 0x07, 0x7F, 0x6F};
  const lv_opa_t colonOpacity = ((millis() / 500U) & 1U) ? LV_OPA_40 : LV_OPA_COVER;
  for (lv_obj_t *dot : clockColon_) if (dot) lv_obj_set_style_bg_opa(dot, colonOpacity, 0);
  for (uint8_t digit = 0; digit < 4; ++digit) {
    if (clockValues_[digit] == values[digit]) continue;
    clockValues_[digit] = values[digit];
    const uint8_t mask = values[digit] >= '0' && values[digit] <= '9'
                             ? masks[values[digit] - '0'] : 0;
    for (uint8_t segment = 0; segment < 7; ++segment) {
      lv_obj_set_style_bg_color(clockSegments_[digit][segment],
                                mask & (1U << segment) ? kMint : kClockOff, 0);
    }
    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, clockDigits_[digit]);
    lv_anim_set_values(&animation, 34, 40);
    lv_anim_set_time(&animation, 180);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&animation, [](void *object, int32_t y) {
      lv_obj_set_y(static_cast<lv_obj_t *>(object), y);
    });
    lv_anim_start(&animation);
  }
  if (clockSyncLabel_) lv_label_set_text(clockSyncLabel_, valid ? "设备时间" : "等待网络对时");
}

void DeviceUi::setScreenBase(lv_obj_t *screen) {
  lv_obj_set_style_bg_color(screen, kBackground, 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
  lv_obj_set_style_text_color(screen, kInk, 0);
  lv_obj_set_style_text_font(screen, font_, 0);
  lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
}

void DeviceUi::addHeader(lv_obj_t *screen, const char *title) {
  lv_obj_t *bar = lv_obj_create(screen);
  lv_obj_set_size(bar, 320, 36);
  lv_obj_set_pos(bar, 0, 0);
  lv_obj_set_style_radius(bar, 0, 0);
  lv_obj_set_style_border_width(bar, 0, 0);
  lv_obj_set_style_bg_color(bar, kMintDark, 0);
  lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t *label = lv_label_create(bar);
  lv_label_set_text(label, title);
  lv_obj_set_style_text_font(label, font_, 0);
  lv_obj_set_style_text_color(label, lv_color_white(), 0);
  lv_obj_align(label, LV_ALIGN_LEFT_MID, 8, 0);
  lv_obj_t *statusPill = lv_obj_create(bar);
  lv_obj_set_size(statusPill, 78, 24);
  lv_obj_align(statusPill, LV_ALIGN_RIGHT_MID, -7, 0);
  lv_obj_set_style_radius(statusPill, 12, 0);
  lv_obj_set_style_border_width(statusPill, 0, 0);
  lv_obj_set_style_bg_color(statusPill, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(statusPill, LV_OPA_20, 0);
  lv_obj_set_style_pad_all(statusPill, 0, 0);
  lv_obj_clear_flag(statusPill, LV_OBJ_FLAG_SCROLLABLE);
  networkStatusDot_ = lv_obj_create(statusPill);
  lv_obj_set_size(networkStatusDot_, 8, 8);
  lv_obj_align(networkStatusDot_, LV_ALIGN_LEFT_MID, 9, 0);
  lv_obj_set_style_radius(networkStatusDot_, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(networkStatusDot_, 0, 0);
  lv_obj_set_style_bg_color(networkStatusDot_, lv_color_white(), 0);
  lv_obj_clear_flag(networkStatusDot_, LV_OBJ_FLAG_SCROLLABLE);
  networkLabel_ = lv_label_create(statusPill);
  lv_obj_set_style_text_font(networkLabel_, smallFont_, 0);
  lv_obj_set_style_text_color(networkLabel_, lv_color_white(), 0);
  lv_obj_align(networkLabel_, LV_ALIGN_LEFT_MID, 23, 0);
}

void DeviceUi::refreshNetworkBadge(const char *networkText) {
  if (!networkLabel_) return;
  const bool station = networkText && strcmp(networkText, "局域网") == 0;
  const bool accessPoint = networkText && strcmp(networkText, "设备热点") == 0;
  const bool connecting = networkText && strcmp(networkText, "连接中") == 0;
  lv_label_set_text(networkLabel_, station ? "WiFi" : accessPoint ? "AP" :
                    connecting ? "..." : "OFF");
  if (networkStatusDot_) {
    lv_obj_set_style_bg_color(networkStatusDot_, accessPoint ? kCoral : lv_color_white(), 0);
    lv_obj_set_style_bg_opa(networkStatusDot_, (station || accessPoint) ? LV_OPA_COVER :
                                                                    LV_OPA_40, 0);
  }
}

void DeviceUi::addTip(lv_obj_t *screen, const char *text) {
  lv_obj_t *pill = lv_obj_create(screen);
  lv_obj_set_size(pill, 300, 24);
  lv_obj_align(pill, LV_ALIGN_BOTTOM_MID, 0, -3);
  lv_obj_set_style_radius(pill, 12, 0);
  lv_obj_set_style_border_width(pill, 0, 0);
  lv_obj_set_style_bg_color(pill, kMintSoft, 0);
  lv_obj_set_style_pad_all(pill, 0, 0);
  lv_obj_clear_flag(pill, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t *label = lv_label_create(pill);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_font(label, smallFont_, 0);
  lv_obj_set_style_text_color(label, kMutedInk, 0);
  lv_obj_center(label);
}

lv_obj_t *DeviceUi::makeButton(lv_obj_t *parent, const char *text, int16_t x, int16_t y,
                               int16_t width, int16_t height, lv_event_cb_t callback,
                               void *userData) {
  lv_obj_t *button = lv_btn_create(parent);
  lv_obj_set_pos(button, x, y);
  lv_obj_set_size(button, width, height);
  lv_obj_set_style_radius(button, 14, 0);
  lv_obj_set_style_bg_color(button, kCream, 0);
  lv_obj_set_style_bg_color(button, kMintSoft, LV_STATE_FOCUSED);
  lv_obj_set_style_bg_color(button, kMint, LV_STATE_PRESSED);
  lv_obj_set_style_border_width(button, 1, 0);
  lv_obj_set_style_border_color(button, kMintSoft, 0);
  lv_obj_set_style_border_width(button, 2, LV_STATE_FOCUSED);
  lv_obj_set_style_border_color(button, kMintDark, LV_STATE_FOCUSED);
  lv_obj_set_style_shadow_width(button, 8, 0);
  lv_obj_set_style_shadow_opa(button, LV_OPA_20, 0);
  lv_obj_set_style_text_color(button, kInk, 0);
  lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, userData);
  lv_obj_t *label = lv_label_create(button);
  lv_label_set_text(label, text);
  lv_obj_center(label);
  return button;
}

lv_obj_t *DeviceUi::makeMenuCard(lv_obj_t *parent, const char *badge, const char *title,
                                 const char *subtitle, int16_t x, int16_t y, UiPage page) {
  static bool focusStylesReady = false;
  static lv_style_t motionStyle;
  static lv_style_t focusStyle;
  static const lv_style_prop_t transitionProps[] = {
      LV_STYLE_TRANSFORM_WIDTH, LV_STYLE_TRANSFORM_HEIGHT, LV_STYLE_SHADOW_WIDTH,
      LV_STYLE_SHADOW_OPA, LV_STYLE_PROP_INV};
  static lv_style_transition_dsc_t focusTransition;
  if (!focusStylesReady) {
    lv_style_init(&motionStyle);
    lv_style_init(&focusStyle);
    lv_style_transition_dsc_init(&focusTransition, transitionProps, lv_anim_path_ease_out, 150, 0,
                                 nullptr);
    lv_style_set_transition(&motionStyle, &focusTransition);
    lv_style_set_transition(&focusStyle, &focusTransition);
    lv_style_set_transform_width(&focusStyle, 4);
    lv_style_set_transform_height(&focusStyle, 3);
    lv_style_set_shadow_width(&focusStyle, 13);
    lv_style_set_shadow_opa(&focusStyle, LV_OPA_20);
    focusStylesReady = true;
  }
  lv_obj_t *button = lv_btn_create(parent);
  lv_obj_set_pos(button, x, y);
  lv_obj_set_size(button, 92, 70);
  lv_obj_set_style_radius(button, 18, 0);
  lv_obj_set_style_bg_color(button, kCream, 0);
  lv_obj_set_style_bg_color(button, kMintSoft, LV_STATE_FOCUSED);
  lv_obj_set_style_bg_color(button, kMint, LV_STATE_PRESSED);
  lv_obj_set_style_border_width(button, 1, 0);
  lv_obj_set_style_border_color(button, LV_COLOR_MAKE(0xDD, 0xE8, 0xE4), 0);
  lv_obj_set_style_border_width(button, 2, LV_STATE_FOCUSED);
  lv_obj_set_style_border_color(button, kMintDark, LV_STATE_FOCUSED);
  lv_obj_set_style_shadow_width(button, 7, 0);
  lv_obj_set_style_shadow_opa(button, LV_OPA_10, 0);
  lv_obj_add_style(button, &motionStyle, LV_STATE_DEFAULT);
  lv_obj_add_style(button, &focusStyle, LV_STATE_FOCUSED);
  lv_obj_set_style_pad_all(button, 0, 0);
  lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(button, menuEvent, LV_EVENT_CLICKED,
                      reinterpret_cast<void *>(static_cast<uintptr_t>(page)));

  lv_obj_t *dot = lv_obj_create(button);
  lv_obj_set_size(dot, 34, 34);
  lv_obj_align(dot, LV_ALIGN_TOP_MID, 0, 6);
  lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(dot, 0, 0);
  lv_obj_set_style_bg_color(dot, LV_COLOR_MAKE(0xD9, 0xEC, 0xFA), 0);
  lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t *badgeLabel = lv_label_create(dot);
  lv_label_set_text(badgeLabel, badge);
  lv_obj_set_style_text_font(badgeLabel, smallFont_, 0);
  lv_obj_set_style_text_color(badgeLabel, LV_COLOR_MAKE(0x2F, 0x69, 0x91), 0);
  lv_obj_center(badgeLabel);

  lv_obj_t *titleLabel = lv_label_create(button);
  lv_label_set_text(titleLabel, title);
  lv_obj_set_style_text_font(titleLabel, smallFont_, 0);
  lv_obj_set_style_text_color(titleLabel, kInk, 0);
  lv_obj_align(titleLabel, LV_ALIGN_BOTTOM_MID, 0, -6);
  (void)subtitle;
  return button;
}

lv_obj_t *DeviceUi::makeActionButton(lv_obj_t *parent, const char *text, int16_t x, int16_t y,
                                     int16_t width, int16_t height, UiAction action) {
  return makeButton(parent, text, x, y, width, height, actionEvent,
                    reinterpret_cast<void *>(static_cast<uintptr_t>(action)));
}

void DeviceUi::buildHome() {
  screen_ = lv_obj_create(nullptr);
  setScreenBase(screen_);
  lv_obj_set_style_bg_color(screen_, LV_COLOR_MAKE(0xEE, 0xE6, 0xD8), 0);
  const lv_color_t warmWhite = LV_COLOR_MAKE(0xFF, 0xFC, 0xF5);
  const lv_color_t orange = LV_COLOR_MAKE(0xE9, 0x78, 0x2D);
  const lv_color_t darkInk = LV_COLOR_MAKE(0x1D, 0x1B, 0x18);
  auto card = [&](int16_t x, int16_t y, int16_t width, int16_t height, int16_t radius) {
    lv_obj_t *object = lv_obj_create(screen_);
    lv_obj_set_pos(object, x, y);
    lv_obj_set_size(object, width, height);
    lv_obj_set_style_radius(object, radius, 0);
    lv_obj_set_style_border_width(object, 0, 0);
    lv_obj_set_style_bg_color(object, warmWhite, 0);
    lv_obj_set_style_shadow_width(object, 11, 0);
    lv_obj_set_style_shadow_opa(object, LV_OPA_20, 0);
    lv_obj_set_style_pad_all(object, 0, 0);
    lv_obj_clear_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    return object;
  };

  weatherCityLabel_ = lv_label_create(screen_);
  lv_obj_set_style_text_font(weatherCityLabel_, titleFont_, 0);
  lv_obj_set_style_text_color(weatherCityLabel_, darkInk, 0);
  lv_obj_set_pos(weatherCityLabel_, 10, 4);
  lv_obj_t *locationDot = lv_obj_create(screen_);
  lv_obj_set_pos(locationDot, 101, 10);
  lv_obj_set_size(locationDot, 8, 8);
  lv_obj_set_style_radius(locationDot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(locationDot, 0, 0);
  lv_obj_set_style_bg_color(locationDot, orange, 0);
  lv_obj_clear_flag(locationDot, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t *cityCn = lv_label_create(screen_);
  lv_label_set_text(cityCn, "上海市");
  lv_obj_set_style_text_font(cityCn, smallFont_, 0);
  lv_obj_set_style_text_color(cityCn, kMutedInk, 0);
  lv_obj_set_pos(cityCn, 10, 27);
  for (uint8_t dot = 0; dot < 4; ++dot) {
    lv_obj_t *pageDot = lv_obj_create(screen_);
    lv_obj_set_pos(pageDot, 10 + dot * 11, 45);
    lv_obj_set_size(pageDot, dot == 0 ? 6 : 4, dot == 0 ? 6 : 4);
    lv_obj_set_style_radius(pageDot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(pageDot, 0, 0);
    lv_color_t dotColor = orange;
    if (dot != 0) dotColor = LV_COLOR_MAKE(0xD8, 0xD0, 0xC4);
    lv_obj_set_style_bg_color(pageDot, dotColor, 0);
    lv_obj_clear_flag(pageDot, LV_OBJ_FLAG_SCROLLABLE);
  }
  timeLabel_ = lv_label_create(screen_);
  lv_obj_set_style_text_font(timeLabel_, titleFont_, 0);
  lv_obj_set_style_text_color(timeLabel_, darkInk, 0);
  lv_obj_set_width(timeLabel_, 92);
  lv_obj_set_style_text_align(timeLabel_, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_pos(timeLabel_, 218, 5);
  weatherDateLabel_ = lv_label_create(screen_);
  lv_obj_set_style_text_font(weatherDateLabel_, smallFont_, 0);
  lv_obj_set_style_text_color(weatherDateLabel_, kMutedInk, 0);
  lv_obj_set_width(weatherDateLabel_, 112);
  lv_obj_set_style_text_align(weatherDateLabel_, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_pos(weatherDateLabel_, 198, 30);

  lv_obj_t *currentCard = card(8, 59, 198, 100, 18);
  weatherScene_ = lv_obj_create(currentCard);
  lv_obj_set_pos(weatherScene_, 7, 10);
  lv_obj_set_size(weatherScene_, 82, 76);
  lv_obj_set_style_radius(weatherScene_, 15, 0);
  lv_obj_set_style_border_width(weatherScene_, 0, 0);
  lv_obj_set_style_bg_color(weatherScene_, warmWhite, 0);
  lv_obj_set_style_pad_all(weatherScene_, 0, 0);
  lv_obj_clear_flag(weatherScene_, LV_OBJ_FLAG_SCROLLABLE);
  weatherConditionLabel_ = lv_label_create(currentCard);
  lv_obj_set_style_text_font(weatherConditionLabel_, smallFont_, 0);
  lv_obj_set_style_text_color(weatherConditionLabel_, kMutedInk, 0);
  lv_obj_set_pos(weatherConditionLabel_, 99, 14);
  weatherTemperatureLabel_ = lv_label_create(currentCard);
  lv_obj_set_style_text_font(weatherTemperatureLabel_, titleFont_, 0);
  lv_obj_set_style_text_color(weatherTemperatureLabel_, darkInk, 0);
  lv_obj_set_pos(weatherTemperatureLabel_, 98, 39);
  weatherSourceLabel_ = lv_label_create(currentCard);
  lv_obj_set_style_text_font(weatherSourceLabel_, smallFont_, 0);
  lv_obj_set_style_text_color(weatherSourceLabel_, orange, 0);
  lv_obj_set_pos(weatherSourceLabel_, 99, 72);

  lv_obj_t *humidityCard = card(212, 59, 100, 47, 15);
  lv_obj_set_style_bg_color(humidityCard, LV_COLOR_MAKE(0xFF, 0xF2, 0xE6), 0);
  lv_obj_t *humidityCaption = lv_label_create(humidityCard);
  lv_label_set_text(humidityCaption, "湿度");
  lv_obj_set_style_text_font(humidityCaption, smallFont_, 0);
  lv_obj_set_style_text_color(humidityCaption, kMutedInk, 0);
  lv_obj_set_pos(humidityCaption, 9, 5);
  weatherHumidityLabel_ = lv_label_create(humidityCard);
  lv_obj_set_style_text_font(weatherHumidityLabel_, font_, 0);
  lv_obj_set_style_text_color(weatherHumidityLabel_, orange, 0);
  lv_obj_set_pos(weatherHumidityLabel_, 44, 21);

  lv_obj_t *aqiCard = card(212, 112, 100, 47, 15);
  lv_obj_set_style_bg_color(aqiCard, LV_COLOR_MAKE(0xF2, 0xF3, 0xE9), 0);
  lv_obj_t *aqiCaption = lv_label_create(aqiCard);
  lv_label_set_text(aqiCaption, "空气质量");
  lv_obj_set_style_text_font(aqiCaption, smallFont_, 0);
  lv_obj_set_style_text_color(aqiCaption, kMutedInk, 0);
  lv_obj_set_pos(aqiCaption, 9, 5);
  weatherAqiLabel_ = lv_label_create(aqiCard);
  lv_obj_set_style_text_font(weatherAqiLabel_, font_, 0);
  lv_obj_set_style_text_color(weatherAqiLabel_, darkInk, 0);
  lv_obj_set_pos(weatherAqiLabel_, 30, 21);
  weatherWindLabel_ = nullptr;

  const int16_t forecastX[3] = {8, 110, 212};
  for (uint8_t index = 0; index < 3; ++index) {
    lv_obj_t *forecastCard = card(forecastX[index], 166, 96, 43, 13);
    forecastDateLabel_[index] = lv_label_create(forecastCard);
    lv_obj_set_style_text_font(forecastDateLabel_[index], smallFont_, 0);
    lv_obj_set_style_text_color(forecastDateLabel_[index], kInk, 0);
    lv_obj_set_pos(forecastDateLabel_[index], 7, 4);
    forecastIcon_[index] = lv_obj_create(forecastCard);
    lv_obj_set_pos(forecastIcon_[index], 7, 20);
    lv_obj_set_size(forecastIcon_[index], 26, 18);
    lv_obj_set_style_border_width(forecastIcon_[index], 0, 0);
    lv_obj_set_style_bg_opa(forecastIcon_[index], LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(forecastIcon_[index], 0, 0);
    lv_obj_clear_flag(forecastIcon_[index], LV_OBJ_FLAG_SCROLLABLE);
    forecastTemperatureLabel_[index] = lv_label_create(forecastCard);
    lv_obj_set_style_text_font(forecastTemperatureLabel_[index], smallFont_, 0);
    lv_obj_set_style_text_color(forecastTemperatureLabel_[index], kInk, 0);
    lv_obj_set_pos(forecastTemperatureLabel_[index], 37, 23);
    setForecast(index, forecastDate_[index], forecastMinimum_[index], forecastMaximum_[index],
                forecastCode_[index]);
  }

  setWeather(weatherCity_, weatherTemperature_, weatherHumidity_, weatherAqi_,
             weatherWindSpeed_, weatherCode_, weatherLive_);
}

void DeviceUi::rebuildWeatherScene() {
  if (!weatherScene_) return;
  lv_obj_clean(weatherScene_);
  auto part = [this](int16_t x, int16_t y, int16_t width, int16_t height, lv_color_t color,
                     int16_t radius = LV_RADIUS_CIRCLE) {
    lv_obj_t *object = lv_obj_create(weatherScene_);
    lv_obj_set_pos(object, x, y);
    lv_obj_set_size(object, width, height);
    lv_obj_set_style_radius(object, radius, 0);
    lv_obj_set_style_border_width(object, 0, 0);
    lv_obj_set_style_bg_color(object, color, 0);
    lv_obj_set_style_pad_all(object, 0, 0);
    lv_obj_clear_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    return object;
  };
  auto sun = [&]() {
    const lv_color_t gold = LV_COLOR_MAKE(0xEA, 0x82, 0x31);
    part(25, 18, 30, 30, gold);
    const int16_t rayX[8] = {37, 57, 66, 57, 37, 17, 8, 17};
    const int16_t rayY[8] = {7, 13, 30, 49, 59, 49, 30, 13};
    for (uint8_t i = 0; i < 8; ++i) {
      lv_obj_t *ray = part(rayX[i], rayY[i], 6, 6, gold);
      lv_anim_t animation;
      lv_anim_init(&animation);
      lv_anim_set_var(&animation, ray);
      lv_anim_set_values(&animation, LV_OPA_40, LV_OPA_COVER);
      lv_anim_set_time(&animation, 650);
      lv_anim_set_delay(&animation, i * 65);
      lv_anim_set_playback_time(&animation, 650);
      lv_anim_set_repeat_count(&animation, LV_ANIM_REPEAT_INFINITE);
      lv_anim_set_exec_cb(&animation, [](void *object, int32_t opacity) {
        lv_obj_set_style_bg_opa(static_cast<lv_obj_t *>(object), opacity, 0);
      });
      lv_anim_start(&animation);
    }
  };
  auto cloud = [&](int16_t offsetX, int16_t offsetY) {
    const lv_color_t cloudGray = LV_COLOR_MAKE(0xC9, 0xC5, 0xBD);
    part(offsetX + 14, offsetY + 8, 30, 19, cloudGray);
    part(offsetX + 3, offsetY + 16, 51, 19, cloudGray, 10);
    part(offsetX + 34, offsetY + 12, 24, 21, cloudGray);
  };

  if (weatherKind_ == WeatherKind::Clear) sun();
  else if (weatherKind_ == WeatherKind::PartlyCloudy) {
    sun();
    cloud(19, 37);
  } else if (weatherKind_ == WeatherKind::Cloudy) {
    cloud(5, 16);
    cloud(20, 36);
  } else if (weatherKind_ == WeatherKind::Fog) {
    cloud(11, 8);
    for (uint8_t i = 0; i < 3; ++i) part(10 + i * 7, 46 + i * 9, 62 - i * 14, 4,
                                        LV_COLOR_MAKE(0xA9, 0xA3, 0x98), 2);
  } else {
    cloud(11, 5);
    if (weatherKind_ == WeatherKind::Thunder) {
      const lv_color_t gold = LV_COLOR_MAKE(0xFF, 0xB9, 0x3E);
      part(38, 36, 10, 18, gold, 3);
      part(32, 50, 10, 17, gold, 3);
    } else {
      lv_color_t particleColor = lv_color_white();
      if (weatherKind_ != WeatherKind::Snow) {
        particleColor = LV_COLOR_MAKE(0x4A, 0x91, 0xC4);
      }
      for (uint8_t i = 0; i < 6; ++i) {
        lv_obj_t *drop = part(9 + i * 11, 38 + (i & 1U) * 6,
                              weatherKind_ == WeatherKind::Snow ? 6 : 3,
                              weatherKind_ == WeatherKind::Snow ? 6 : 11,
                              particleColor, 4);
        const int16_t startY = lv_obj_get_y(drop);
        lv_anim_t animation;
        lv_anim_init(&animation);
        lv_anim_set_var(&animation, drop);
        lv_anim_set_values(&animation, startY, startY + 18);
        lv_anim_set_time(&animation, weatherKind_ == WeatherKind::Snow ? 1200 : 650);
        lv_anim_set_delay(&animation, i * 90);
        lv_anim_set_repeat_count(&animation, LV_ANIM_REPEAT_INFINITE);
        lv_anim_set_exec_cb(&animation, [](void *object, int32_t y) {
          lv_obj_set_y(static_cast<lv_obj_t *>(object), y);
        });
        lv_anim_start(&animation);
      }
    }
  }
}

void DeviceUi::rebuildForecastIcon(uint8_t index) {
  if (index >= 3 || !forecastIcon_[index]) return;
  lv_obj_clean(forecastIcon_[index]);
  const WeatherKind kind = weatherKindFromCode(forecastCode_[index]);
  auto tiny = [&](int16_t x, int16_t y, int16_t width, int16_t height, lv_color_t color) {
    lv_obj_t *object = lv_obj_create(forecastIcon_[index]);
    lv_obj_set_pos(object, x, y);
    lv_obj_set_size(object, width, height);
    lv_obj_set_style_radius(object, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(object, 0, 0);
    lv_obj_set_style_bg_color(object, color, 0);
    lv_obj_set_style_pad_all(object, 0, 0);
    lv_obj_clear_flag(object, LV_OBJ_FLAG_SCROLLABLE);
  };
  const lv_color_t orange = LV_COLOR_MAKE(0xE3, 0x86, 0x40);
  const lv_color_t cloud = LV_COLOR_MAKE(0xD8, 0xD4, 0xCC);
  if (kind == WeatherKind::Clear || kind == WeatherKind::PartlyCloudy) tiny(2, 1, 12, 12, orange);
  if (kind != WeatherKind::Clear) {
    tiny(8, 7, 14, 10, cloud);
    tiny(3, 10, 22, 8, cloud);
  }
}

void DeviceUi::setForecast(uint8_t index, const char *date, int8_t minimum, int8_t maximum,
                           uint8_t weatherCode) {
  if (index >= 3) return;
  if (date && date[0]) strlcpy(forecastDate_[index], date, sizeof(forecastDate_[index]));
  forecastMinimum_[index] = minimum;
  forecastMaximum_[index] = maximum;
  forecastCode_[index] = weatherCode;
  if (!forecastDateLabel_[index]) return;
  lv_label_set_text(forecastDateLabel_[index], forecastDate_[index]);
  char text[24]{};
  snprintf(text, sizeof(text), "%d° / %d°", minimum, maximum);
  lv_label_set_text(forecastTemperatureLabel_[index], text);
  rebuildForecastIcon(index);
}

void DeviceUi::setWeather(const char *city, int16_t temperature, uint8_t humidity, uint16_t aqi,
                          uint16_t windSpeed, uint8_t weatherCode, bool live) {
  if (city && city[0]) strlcpy(weatherCity_, city, sizeof(weatherCity_));
  weatherTemperature_ = temperature;
  weatherHumidity_ = humidity;
  weatherAqi_ = aqi;
  weatherWindSpeed_ = windSpeed;
  weatherCode_ = weatherCode;
  weatherLive_ = live;
  weatherKind_ = live ? weatherKindFromCode(weatherCode)
                      : static_cast<WeatherKind>(weatherCode % 7);
  if (!weatherScene_) return;
  char text[40]{};
  snprintf(text, sizeof(text), "%d°C", weatherTemperature_);
  lv_label_set_text(weatherTemperatureLabel_, text);
  lv_label_set_text(weatherCityLabel_, weatherCity_);
  lv_label_set_text(weatherConditionLabel_, weatherName(weatherKind_));
  lv_label_set_text(weatherSourceLabel_, weatherLive_ ? "实时天气" : "离线预览");
  snprintf(text, sizeof(text), "%u%%", weatherHumidity_);
  lv_label_set_text(weatherHumidityLabel_, text);
  if (weatherAqi_) snprintf(text, sizeof(text), "AQI %u", weatherAqi_);
  else strlcpy(text, "AQI --", sizeof(text));
  lv_label_set_text(weatherAqiLabel_, text);
  if (weatherWindLabel_) {
    snprintf(text, sizeof(text), "%u km/h", weatherWindSpeed_);
    lv_label_set_text(weatherWindLabel_, text);
  }
  rebuildWeatherScene();
}

void DeviceUi::showWeatherPreview(int8_t direction) {
  const int8_t count = 7;
  weatherPreviewIndex_ = static_cast<uint8_t>((weatherPreviewIndex_ + direction + count) % count);
  static const int8_t temperatures[7] = {28, 26, 24, 21, 23, -2, 18};
  static const uint8_t humidity[7] = {45, 58, 64, 86, 82, 79, 91};
  setWeather("天气预览", temperatures[weatherPreviewIndex_], humidity[weatherPreviewIndex_],
             32 + weatherPreviewIndex_ * 5, 8 + weatherPreviewIndex_ * 2,
             weatherPreviewIndex_, false);
}

void DeviceUi::buildMenu() {
  screen_ = lv_obj_create(nullptr);
  setScreenBase(screen_);
  addHeader(screen_, "应用");
  menuCards_[0] = makeMenuCard(screen_, "12", "时钟", "蓝白翻页时钟", 10, 42, UiPage::Clock);
  menuCards_[1] = makeMenuCard(screen_, "T", "待办", "待办与会议", 114, 42, UiPage::Tasks);
  menuCards_[2] = makeMenuCard(screen_, "N", "网络", "Wi-Fi 与热点", 218, 42, UiPage::Network);
  menuCards_[3] = makeMenuCard(screen_, "G", "游戏", "三个益智游戏", 10, 120, UiPage::Games);
  menuCards_[4] = makeMenuCard(screen_, "S", "设置", "声音与设备", 114, 120, UiPage::Settings);
  menuCards_[5] = makeMenuCard(screen_, "H", "首页", "返回今日首页", 218, 120, UiPage::Home);
  menuFocus_ = 0;
  lv_group_focus_obj(menuCards_[menuFocus_]);
  lv_obj_t *hint = lv_obj_create(screen_);
  lv_obj_set_size(hint, 300, 24);
  lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -3);
  lv_obj_set_style_radius(hint, 12, 0);
  lv_obj_set_style_border_width(hint, 0, 0);
  lv_obj_set_style_bg_color(hint, LV_COLOR_MAKE(0xE7, 0xF2, 0xFA), 0);
  lv_obj_set_style_pad_all(hint, 0, 0);
  lv_obj_clear_flag(hint, LV_OBJ_FLAG_SCROLLABLE);
  menuHintLabel_ = lv_label_create(hint);
  lv_obj_set_style_text_font(menuHintLabel_, smallFont_, 0);
  lv_obj_set_style_text_color(menuHintLabel_, LV_COLOR_MAKE(0x4A, 0x70, 0x88), 0);
  lv_obj_center(menuHintLabel_);
  refreshMenuFocus();
}

void DeviceUi::refreshMenuFocus() {
  static const char *hints[6] = {
      "自动对时 · 按下进入", "查看待办与会议 · 按下进入", "局域网和热点 · 按下进入",
      "三个离线益智游戏 · 按下进入", "声音和设备选项 · 按下进入", "回到今日首页 · 按下进入"};
  if (menuHintLabel_ && menuFocus_ < 6) lv_label_set_text(menuHintLabel_, hints[menuFocus_]);
}

void DeviceUi::buildDetail(UiPage page) {
  if (page == UiPage::Clock) {
    buildClock(false);
    return;
  }
  screen_ = lv_obj_create(nullptr);
  setScreenBase(screen_);
  addHeader(screen_, pageTitle(page));
  detailLabel_ = lv_label_create(screen_);
  lv_obj_set_pos(detailLabel_, 14, 52);
  lv_obj_set_width(detailLabel_, 292);
  if (page == UiPage::Tasks) {
    taskListLabel_ = detailLabel_;
    makeButton(screen_, "完成下一项", 86, 169, 148, 40, completeNextEvent, this);
    addTip(screen_, "小贴士：完成后会自动显示下一项");
    refreshTasks();
  } else if (page == UiPage::Network) {
    lv_obj_del(detailLabel_);
    lv_obj_t *statusCard = lv_obj_create(screen_);
    lv_obj_set_pos(statusCard, 12, 46);
    lv_obj_set_size(statusCard, 296, 78);
    lv_obj_set_style_radius(statusCard, 14, 0);
    lv_obj_set_style_border_width(statusCard, 1, 0);
    lv_obj_set_style_border_color(statusCard, LV_COLOR_MAKE(0xD5, 0xE5, 0xED), 0);
    lv_obj_set_style_bg_color(statusCard, kCream, 0);
    lv_obj_set_style_pad_all(statusCard, 0, 0);
    lv_obj_clear_flag(statusCard, LV_OBJ_FLAG_SCROLLABLE);
    detailLabel_ = lv_label_create(statusCard);
    lv_label_set_text(detailLabel_, networkSummary_[0] ? networkSummary_ : "正在读取网络状态");
    lv_obj_set_style_text_font(detailLabel_, smallFont_, 0);
    lv_obj_set_style_text_color(detailLabel_, kMintDark, 0);
    lv_obj_align(detailLabel_, LV_ALIGN_LEFT_MID, 12, 0);
    makeActionButton(screen_, "连接 Wi-Fi", 12, 134, 92, 44, UiAction::NetworkStation);
    makeActionButton(screen_, "开启热点", 114, 134, 92, 44, UiAction::NetworkAccessPoint);
    makeActionButton(screen_, "离线模式", 216, 134, 92, 44, UiAction::NetworkOffline);
    addTip(screen_, "小贴士：离线时待办和提醒仍可使用");
  } else if (page == UiPage::Games) {
    lv_label_set_text(detailLabel_, "选择一个游戏，全部可以离线运行");
    makeButton(screen_, "点灯", 18, 82, 136, 48, menuEvent,
               reinterpret_cast<void *>(static_cast<uintptr_t>(UiPage::GameLights)));
    makeButton(screen_, "记忆翻牌", 166, 82, 136, 48, menuEvent,
               reinterpret_cast<void *>(static_cast<uintptr_t>(UiPage::GameMemory)));
    makeButton(screen_, "打砖块", 18, 140, 136, 48, menuEvent,
               reinterpret_cast<void *>(static_cast<uintptr_t>(UiPage::GameBreakout)));
    makeButton(screen_, "俄罗斯方块", 166, 140, 136, 48, menuEvent,
               reinterpret_cast<void *>(static_cast<uintptr_t>(UiPage::GameTetris)));
    addTip(screen_, "小贴士：游戏进度不会影响提醒功能");
  } else if (page == UiPage::GameLights) {
    lv_obj_del(detailLabel_);
    detailLabel_ = nullptr;
    for (uint8_t row = 0; row < 3; ++row) {
      for (uint8_t column = 0; column < 3; ++column) {
        const uint8_t index = row * 3 + column;
        lv_obj_t *cell = lv_btn_create(screen_);
        gameCells_[index] = cell;
        lv_obj_set_pos(cell, 18 + column * 48, 51 + row * 48);
        lv_obj_set_size(cell, 42, 42);
        lv_obj_set_style_radius(cell, 12, 0);
        lv_obj_set_style_shadow_width(cell, 0, 0);
        lv_obj_set_style_border_width(cell, 1, 0);
        lv_obj_set_style_border_color(cell, LV_COLOR_MAKE(0xD7, 0xE5, 0xE1), 0);
        lv_obj_set_style_border_width(cell, 3, LV_STATE_FOCUSED);
        lv_obj_set_style_border_color(cell, kMintDark, LV_STATE_FOCUSED);
        lv_obj_add_event_cb(cell, gameCellEvent, LV_EVENT_CLICKED,
                            reinterpret_cast<void *>(static_cast<uintptr_t>(index)));
      }
    }
    gameStatusLabel_ = lv_label_create(screen_);
    lv_obj_set_pos(gameStatusLabel_, 176, 62);
    lv_obj_set_width(gameStatusLabel_, 130);
    lv_obj_set_style_text_color(gameStatusLabel_, kMintDark, 0);
    makeButton(screen_, "重新开始", 176, 135, 126, 44, gameResetEvent, this);
    addTip(screen_, "小贴士：每次会翻转相邻灯格");
    refreshGame();
  } else if (page == UiPage::GameMemory) {
    lv_obj_del(detailLabel_);
    detailLabel_ = nullptr;
    for (uint8_t index = 0; index < 8; ++index) {
      const uint8_t row = index / 4;
      const uint8_t column = index % 4;
      lv_obj_t *cell = lv_btn_create(screen_);
      memoryCells_[index] = cell;
      lv_obj_set_pos(cell, 23 + column * 70, 50 + row * 67);
      lv_obj_set_size(cell, 62, 59);
      lv_obj_set_style_radius(cell, 13, 0);
      lv_obj_set_style_shadow_width(cell, 0, 0);
      lv_obj_set_style_border_width(cell, 2, LV_STATE_FOCUSED);
      lv_obj_set_style_border_color(cell, kMintDark, LV_STATE_FOCUSED);
      lv_obj_add_event_cb(cell, memoryCellEvent, LV_EVENT_CLICKED,
                          reinterpret_cast<void *>(static_cast<uintptr_t>(index)));
      lv_obj_t *label = lv_label_create(cell);
      memoryLabels_[index] = label;
      lv_obj_set_style_text_font(label, titleFont_, 0);
      lv_obj_center(label);
    }
    gameStatusLabel_ = lv_label_create(screen_);
    lv_obj_set_pos(gameStatusLabel_, 23, 187);
    makeButton(screen_, "重新洗牌", 207, 181, 91, 30, memoryResetEvent, this);
    addTip(screen_, "小贴士：找出四组相同数字");
    resetMemoryGame();
  } else if (page == UiPage::GameBreakout) {
    lv_obj_del(detailLabel_);
    detailLabel_ = nullptr;
    breakoutBoard_ = lv_obj_create(screen_);
    lv_obj_set_pos(breakoutBoard_, 12, 47);
    lv_obj_set_size(breakoutBoard_, 222, 150);
    lv_obj_set_style_radius(breakoutBoard_, 8, 0);
    lv_obj_set_style_border_width(breakoutBoard_, 2, 0);
    lv_obj_set_style_border_color(breakoutBoard_, kMintDark, 0);
    lv_obj_set_style_bg_color(breakoutBoard_, LV_COLOR_MAKE(0x17, 0x2E, 0x38), 0);
    lv_obj_set_style_pad_all(breakoutBoard_, 0, 0);
    lv_obj_clear_flag(breakoutBoard_, LV_OBJ_FLAG_SCROLLABLE);
    const lv_color_t brickColors[3] = {kCoral, LV_COLOR_MAKE(0xF3, 0xBA, 0x65), kMint};
    for (uint8_t index = 0; index < 18; ++index) {
      breakoutBricks_[index] = lv_obj_create(breakoutBoard_);
      lv_obj_set_pos(breakoutBricks_[index], 7 + (index % 6) * 35, 7 + (index / 6) * 15);
      lv_obj_set_size(breakoutBricks_[index], 30, 10);
      lv_obj_set_style_radius(breakoutBricks_[index], 3, 0);
      lv_obj_set_style_border_width(breakoutBricks_[index], 0, 0);
      lv_obj_set_style_bg_color(breakoutBricks_[index], brickColors[index / 6], 0);
    }
    breakoutPaddle_ = lv_obj_create(breakoutBoard_);
    lv_obj_set_size(breakoutPaddle_, 52, 7);
    lv_obj_set_style_radius(breakoutPaddle_, 4, 0);
    lv_obj_set_style_border_width(breakoutPaddle_, 0, 0);
    lv_obj_set_style_bg_color(breakoutPaddle_, lv_color_white(), 0);
    breakoutBall_ = lv_obj_create(breakoutBoard_);
    lv_obj_set_size(breakoutBall_, 8, 8);
    lv_obj_set_style_radius(breakoutBall_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(breakoutBall_, 0, 0);
    lv_obj_set_style_bg_color(breakoutBall_, kCoral, 0);
    gameStatusLabel_ = lv_label_create(screen_);
    lv_obj_set_pos(gameStatusLabel_, 244, 66);
    lv_obj_set_width(gameStatusLabel_, 70);
    addTip(screen_, "摇杆移动挡板 · 按下发球");
    resetBreakout();
  } else if (page == UiPage::GameTetris) {
    lv_obj_del(detailLabel_);
    detailLabel_ = nullptr;
    tetrisBoard_ = lv_obj_create(screen_);
    lv_obj_set_pos(tetrisBoard_, 20, 47);
    lv_obj_set_size(tetrisBoard_, 114, 162);
    lv_obj_set_style_radius(tetrisBoard_, 7, 0);
    lv_obj_set_style_border_width(tetrisBoard_, 2, 0);
    lv_obj_set_style_border_color(tetrisBoard_, kMintDark, 0);
    lv_obj_set_style_bg_color(tetrisBoard_, LV_COLOR_MAKE(0x17, 0x2E, 0x38), 0);
    lv_obj_set_style_pad_all(tetrisBoard_, 7, 0);
    lv_obj_clear_flag(tetrisBoard_, LV_OBJ_FLAG_SCROLLABLE);
    gameStatusLabel_ = lv_label_create(screen_);
    lv_obj_set_pos(gameStatusLabel_, 158, 65);
    lv_obj_set_width(gameStatusLabel_, 145);
    lv_obj_set_style_text_color(gameStatusLabel_, kMintDark, 0);
    addTip(screen_, "左右移动 · 上旋转 · 下加速");
    resetTetris();
  } else {
    lv_label_set_text(detailLabel_, "提示音与设备状态");
    makeActionButton(screen_, muted_ ? "开启声音" : "静音", 28, 105, 120, 50,
                     UiAction::ToggleMute);
    makeActionButton(screen_, "试听提示音", 172, 105, 120, 50, UiAction::TestSound);
    addTip(screen_, "小贴士：试听前请先开启声音");
  }
}

void DeviceUi::menuEvent(lv_event_t *event) {
  const uintptr_t raw = reinterpret_cast<uintptr_t>(lv_event_get_user_data(event));
  // Menu callbacks store only the target page. The active instance is recovered from the
  // screen's user data so buttons do not need per-item allocations.
  lv_obj_t *target = lv_event_get_target(event);
  DeviceUi *owner = static_cast<DeviceUi *>(lv_obj_get_user_data(lv_obj_get_screen(target)));
  if (owner) owner->show(static_cast<UiPage>(raw));
}

void DeviceUi::actionEvent(lv_event_t *event) {
  lv_obj_t *target = lv_event_get_target(event);
  DeviceUi *owner = static_cast<DeviceUi *>(lv_obj_get_user_data(lv_obj_get_screen(target)));
  if (!owner) return;
  const UiAction action = static_cast<UiAction>(
      reinterpret_cast<uintptr_t>(lv_event_get_user_data(event)));
  if (action == UiAction::NetworkStation || action == UiAction::NetworkAccessPoint) {
    owner->showActionDialog(action);
  } else {
    owner->pendingAction_ = action;
  }
  owner->lastInteractionMs_ = millis();
}

void DeviceUi::showActionDialog(UiAction action) {
  if (actionDialog_) return;
  confirmAction_ = action;
  actionDialog_ = lv_obj_create(screen_);
  lv_obj_set_size(actionDialog_, 320, 240);
  lv_obj_set_pos(actionDialog_, 0, 0);
  lv_obj_set_style_radius(actionDialog_, 0, 0);
  lv_obj_set_style_border_width(actionDialog_, 0, 0);
  lv_obj_set_style_bg_color(actionDialog_, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(actionDialog_, LV_OPA_40, 0);
  lv_obj_set_style_pad_all(actionDialog_, 0, 0);
  lv_obj_clear_flag(actionDialog_, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *panel = lv_obj_create(actionDialog_);
  lv_obj_set_size(panel, 286, 154);
  lv_obj_center(panel);
  lv_obj_set_style_radius(panel, 18, 0);
  lv_obj_set_style_border_width(panel, 1, 0);
  lv_obj_set_style_border_color(panel, LV_COLOR_MAKE(0xC8, 0xDD, 0xE8), 0);
  lv_obj_set_style_bg_color(panel, kCream, 0);
  lv_obj_set_style_shadow_width(panel, 18, 0);
  lv_obj_set_style_shadow_opa(panel, LV_OPA_30, 0);
  lv_obj_set_style_pad_all(panel, 0, 0);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

  const bool station = action == UiAction::NetworkStation;
  lv_obj_t *title = lv_label_create(panel);
  lv_label_set_text(title, station ? "连接局域网" : "开启设备热点");
  lv_obj_set_style_text_font(title, titleFont_, 0);
  lv_obj_set_style_text_color(title, kMintDark, 0);
  lv_obj_set_pos(title, 16, 13);
  lv_obj_t *message = lv_label_create(panel);
  lv_label_set_text(message, station
      ? "连接已保存的 Wi-Fi？\n成功后请使用屏幕显示的 IP 访问网页。"
      : "切换到设备热点？\n完成后访问 http://192.168.5.1");
  lv_obj_set_style_text_font(message, smallFont_, 0);
  lv_obj_set_style_text_color(message, kMutedInk, 0);
  lv_obj_set_width(message, 254);
  lv_obj_set_pos(message, 16, 49);
  makeButton(panel, "取消", 20, 104, 112, 38, cancelActionEvent, this);
  lv_obj_t *confirm = makeButton(panel, "确认", 154, 104, 112, 38,
                                 confirmActionEvent, this);
  lv_group_focus_obj(confirm);
}

void DeviceUi::closeActionDialog() {
  if (!actionDialog_) return;
  lv_obj_t *dialog = actionDialog_;
  actionDialog_ = nullptr;
  lv_obj_del_async(dialog);
}

void DeviceUi::confirmActionEvent(lv_event_t *event) {
  DeviceUi *owner = static_cast<DeviceUi *>(lv_event_get_user_data(event));
  if (!owner) return;
  owner->pendingAction_ = owner->confirmAction_;
  owner->confirmAction_ = UiAction::None;
  owner->closeActionDialog();
  owner->lastInteractionMs_ = millis();
}

void DeviceUi::cancelActionEvent(lv_event_t *event) {
  DeviceUi *owner = static_cast<DeviceUi *>(lv_event_get_user_data(event));
  if (!owner) return;
  owner->confirmAction_ = UiAction::None;
  owner->closeActionDialog();
  owner->lastInteractionMs_ = millis();
}

UiAction DeviceUi::takeAction() {
  const UiAction action = pendingAction_;
  pendingAction_ = UiAction::None;
  return action;
}

void DeviceUi::completeNextEvent(lv_event_t *event) {
  DeviceUi *self = static_cast<DeviceUi *>(lv_event_get_user_data(event));
  uint8_t indices[kMaxTasks];
  const uint8_t count = self->taskService_.sortedPendingIndices(indices, kMaxTasks);
  if (count) {
    const uint32_t id = self->tasks_.items[indices[0]].id;
    self->taskService_.setStatus(id, TaskStatus::Completed, 0);
    self->store_.save(self->tasks_);
    self->refreshTasks();
  }
}

void DeviceUi::gameCellEvent(lv_event_t *event) {
  static constexpr uint16_t masks[9] = {
      0x00B, 0x017, 0x026,
      0x059, 0x0BA, 0x134,
      0x0C8, 0x1D0, 0x1A0,
  };
  lv_obj_t *target = lv_event_get_target(event);
  DeviceUi *owner = static_cast<DeviceUi *>(lv_obj_get_user_data(lv_obj_get_screen(target)));
  const uint8_t index = static_cast<uint8_t>(
      reinterpret_cast<uintptr_t>(lv_event_get_user_data(event)));
  if (!owner || index >= 9) return;
  owner->gameLights_ ^= masks[index];
  ++owner->gameMoves_;
  owner->lastInteractionMs_ = millis();
  owner->refreshGame();
}

void DeviceUi::gameResetEvent(lv_event_t *event) {
  DeviceUi *owner = static_cast<DeviceUi *>(lv_event_get_user_data(event));
  if (owner) owner->resetGame();
}

void DeviceUi::resetGame() {
  gameLights_ = 0x111;
  gameMoves_ = 0;
  lastInteractionMs_ = millis();
  refreshGame();
}

void DeviceUi::refreshGame() {
  for (uint8_t index = 0; index < 9; ++index) {
    if (!gameCells_[index]) continue;
    const bool lit = (gameLights_ & (1U << index)) != 0;
    lv_obj_set_style_bg_color(gameCells_[index], lit ? kCoral : kMintSoft, 0);
  }
  if (!gameStatusLabel_) return;
  char status[64]{};
  if (gameLights_ == 0) {
    snprintf(status, sizeof(status), "完成！\n用了 %u 步", gameMoves_);
  } else {
    snprintf(status, sizeof(status), "点灯谜题\n步数 %u\n\n熄灭所有灯", gameMoves_);
  }
  lv_label_set_text(gameStatusLabel_, status);
}

void DeviceUi::memoryCellEvent(lv_event_t *event) {
  lv_obj_t *target = lv_event_get_target(event);
  DeviceUi *owner = static_cast<DeviceUi *>(lv_obj_get_user_data(lv_obj_get_screen(target)));
  const uint8_t index = static_cast<uint8_t>(
      reinterpret_cast<uintptr_t>(lv_event_get_user_data(event)));
  if (!owner || index >= 8 || owner->memoryHideAtMs_ ||
      (owner->memoryMatched_ & (1U << index)) || (owner->memoryVisible_ & (1U << index))) return;
  owner->memoryVisible_ |= 1U << index;
  if (owner->memoryFirst_ < 0) {
    owner->memoryFirst_ = index;
  } else {
    const uint8_t first = static_cast<uint8_t>(owner->memoryFirst_);
    if (owner->memoryValues_[first] == owner->memoryValues_[index]) {
      owner->memoryMatched_ |= (1U << first) | (1U << index);
      owner->memoryVisible_ &= ~((1U << first) | (1U << index));
      owner->memoryFirst_ = -1;
    } else {
      owner->memoryHideAtMs_ = millis() + 700;
    }
  }
  owner->lastInteractionMs_ = millis();
  owner->refreshMemoryGame();
}

void DeviceUi::memoryResetEvent(lv_event_t *event) {
  DeviceUi *owner = static_cast<DeviceUi *>(lv_event_get_user_data(event));
  if (owner) owner->resetMemoryGame();
}

void DeviceUi::resetMemoryGame() {
  const uint8_t initial[8] = {1, 2, 3, 4, 1, 2, 3, 4};
  memcpy(memoryValues_, initial, sizeof(memoryValues_));
  uint32_t seed = millis() ^ 0x5A17U;
  for (int8_t index = 7; index > 0; --index) {
    seed = seed * 1664525UL + 1013904223UL;
    const uint8_t target = seed % (index + 1);
    const uint8_t value = memoryValues_[index];
    memoryValues_[index] = memoryValues_[target];
    memoryValues_[target] = value;
  }
  memoryMatched_ = 0;
  memoryVisible_ = 0;
  memoryFirst_ = -1;
  memoryHideAtMs_ = 0;
  refreshMemoryGame();
}

void DeviceUi::refreshMemoryGame() {
  uint8_t pairs = 0;
  for (uint8_t index = 0; index < 8; ++index) {
    if (!memoryLabels_[index]) continue;
    const bool matched = memoryMatched_ & (1U << index);
    const bool visible = memoryVisible_ & (1U << index);
    char value[2] = {'?', '\0'};
    if (matched || visible) value[0] = static_cast<char>('0' + memoryValues_[index]);
    lv_label_set_text(memoryLabels_[index], value);
    lv_obj_set_style_bg_color(memoryCells_[index], matched ? kMint :
                              (visible ? kCoral : kCream), 0);
    if (matched) ++pairs;
  }
  if (gameStatusLabel_) {
    char status[48]{};
    snprintf(status, sizeof(status), memoryMatched_ == 0xFF ? "全部配对完成" :
             "已找到 %u 组", pairs / 2);
    lv_label_set_text(gameStatusLabel_, status);
  }
}

void DeviceUi::resetBreakout() {
  breakoutBricksAlive_ = (1UL << 18) - 1;
  breakoutPaddleX_ = 83;
  breakoutBallX_ = 105;
  breakoutBallY_ = 124;
  breakoutBallDx_ = 2;
  breakoutBallDy_ = -2;
  breakoutScore_ = 0;
  breakoutRunning_ = false;
  breakoutLastTickMs_ = millis();
  refreshBreakout();
}

void DeviceUi::refreshBreakout() {
  if (!breakoutBoard_) return;
  lv_obj_set_pos(breakoutPaddle_, breakoutPaddleX_, 137);
  lv_obj_set_pos(breakoutBall_, breakoutBallX_, breakoutBallY_);
  for (uint8_t index = 0; index < 18; ++index) {
    if (breakoutBricksAlive_ & (1UL << index))
      lv_obj_clear_flag(breakoutBricks_[index], LV_OBJ_FLAG_HIDDEN);
    else
      lv_obj_add_flag(breakoutBricks_[index], LV_OBJ_FLAG_HIDDEN);
  }
  char status[64]{};
  snprintf(status, sizeof(status), "得分\n%u\n\n%s", breakoutScore_,
           breakoutRunning_ ? "接住球！" : "按摇杆\n开始");
  lv_label_set_text(gameStatusLabel_, status);
}

void DeviceUi::updateBreakout(uint32_t now) {
  if (!breakoutRunning_ || now - breakoutLastTickMs_ < 28) return;
  breakoutLastTickMs_ = now;
  int16_t nextX = breakoutBallX_ + breakoutBallDx_;
  int16_t nextY = breakoutBallY_ + breakoutBallDy_;
  if (nextX <= 1 || nextX >= 211) { breakoutBallDx_ = -breakoutBallDx_; nextX = breakoutBallX_ + breakoutBallDx_; }
  if (nextY <= 1) { breakoutBallDy_ = -breakoutBallDy_; nextY = breakoutBallY_ + breakoutBallDy_; }
  if (breakoutBallDy_ > 0 && nextY >= 129 && breakoutBallY_ < 137 &&
      nextX + 8 >= breakoutPaddleX_ && nextX <= breakoutPaddleX_ + 52) {
    breakoutBallDy_ = -2;
    breakoutBallDx_ = nextX < breakoutPaddleX_ + 26 ? -2 : 2;
    nextY = 128;
  }
  for (uint8_t index = 0; index < 18; ++index) {
    if (!(breakoutBricksAlive_ & (1UL << index))) continue;
    const int16_t bx = 7 + (index % 6) * 35;
    const int16_t by = 7 + (index / 6) * 15;
    if (nextX + 8 >= bx && nextX <= bx + 30 && nextY + 8 >= by && nextY <= by + 10) {
      breakoutBricksAlive_ &= ~(1UL << index);
      breakoutBallDy_ = -breakoutBallDy_;
      ++breakoutScore_;
      break;
    }
  }
  breakoutBallX_ = nextX;
  breakoutBallY_ = nextY;
  if (breakoutBallY_ > 145) {
    breakoutRunning_ = false;
    breakoutBallX_ = breakoutPaddleX_ + 22;
    breakoutBallY_ = 124;
  } else if (!breakoutBricksAlive_) {
    resetBreakout();
    return;
  }
  refreshBreakout();
}

bool DeviceUi::tetrisCanPlace(int8_t x, int8_t y, uint8_t rotation) const {
  const uint16_t mask = kTetrisShapes[tetrisShape_][rotation & 3U];
  for (uint8_t row = 0; row < 4; ++row) for (uint8_t column = 0; column < 4; ++column) {
    if (!(mask & (1U << (row * 4 + column)))) continue;
    const int8_t px = x + column, py = y + row;
    if (px < 0 || px >= 8 || py >= 12) return false;
    if (py >= 0 && (tetrisRows_[py] & (1U << px))) return false;
  }
  return true;
}

void DeviceUi::spawnTetrisPiece() {
  tetrisShape_ = static_cast<uint8_t>((millis() + tetrisScore_ * 17U) % 5U);
  tetrisRotation_ = 0;
  tetrisX_ = 2;
  tetrisY_ = 0;
  if (!tetrisCanPlace(tetrisX_, tetrisY_, tetrisRotation_)) tetrisGameOver_ = true;
}

void DeviceUi::resetTetris() {
  memset(tetrisRows_, 0, sizeof(tetrisRows_));
  tetrisScore_ = 0;
  tetrisGameOver_ = false;
  tetrisLastTickMs_ = millis();
  spawnTetrisPiece();
  refreshTetris();
}

void DeviceUi::lockTetrisPiece() {
  const uint16_t mask = kTetrisShapes[tetrisShape_][tetrisRotation_];
  for (uint8_t row = 0; row < 4; ++row) for (uint8_t column = 0; column < 4; ++column) {
    if (!(mask & (1U << (row * 4 + column)))) continue;
    const int8_t px = tetrisX_ + column, py = tetrisY_ + row;
    if (py >= 0 && py < 12) tetrisRows_[py] |= (1U << px);
  }
  for (int8_t row = 11; row >= 0; --row) {
    if (tetrisRows_[row] != 0xFF) continue;
    for (int8_t copy = row; copy > 0; --copy) tetrisRows_[copy] = tetrisRows_[copy - 1];
    tetrisRows_[0] = 0;
    tetrisScore_ += 10;
    ++row;
  }
  spawnTetrisPiece();
}

void DeviceUi::updateTetris(uint32_t now) {
  if (tetrisGameOver_ || now - tetrisLastTickMs_ < 520) return;
  tetrisLastTickMs_ = now;
  if (tetrisCanPlace(tetrisX_, tetrisY_ + 1, tetrisRotation_)) ++tetrisY_;
  else lockTetrisPiece();
  refreshTetris();
}

void DeviceUi::refreshTetris() {
  if (!tetrisBoard_) return;
  lv_obj_clean(tetrisBoard_);
  auto drawBlock = [this](uint8_t column, uint8_t row, lv_color_t color) {
    lv_obj_t *block = lv_obj_create(tetrisBoard_);
    lv_obj_set_pos(block, 2 + column * 12, 2 + row * 12);
    lv_obj_set_size(block, 10, 10);
    lv_obj_set_style_radius(block, 2, 0);
    lv_obj_set_style_border_width(block, 0, 0);
    lv_obj_set_style_bg_color(block, color, 0);
  };
  for (uint8_t row = 0; row < 12; ++row) for (uint8_t column = 0; column < 8; ++column)
    if (tetrisRows_[row] & (1U << column)) drawBlock(column, row, kMint);
  if (!tetrisGameOver_) {
    const uint16_t mask = kTetrisShapes[tetrisShape_][tetrisRotation_];
    for (uint8_t row = 0; row < 4; ++row) for (uint8_t column = 0; column < 4; ++column) {
      if (!(mask & (1U << (row * 4 + column)))) continue;
      const int8_t px = tetrisX_ + column, py = tetrisY_ + row;
      if (px >= 0 && py >= 0 && px < 8 && py < 12) drawBlock(px, py, kCoral);
    }
  }
  char status[72]{};
  snprintf(status, sizeof(status), tetrisGameOver_ ? "游戏结束\n\n得分 %u\n\n按下重开" :
           "得分 %u\n\n上：旋转\n下：加速", tetrisScore_);
  lv_label_set_text(gameStatusLabel_, status);
}

void DeviceUi::refreshHome() {
  if (!taskTitleLabel_) return;
  uint8_t indices[kMaxTasks];
  const uint8_t count = taskService_.sortedPendingIndices(indices, kMaxTasks);
  if (!count) {
    lv_label_set_text(taskTitleLabel_, "暂时没有待办");
    lv_label_set_text(taskTimeLabel_, "去菜单看看，或打开本地网页添加");
    return;
  }
  const Task &task = tasks_.items[indices[0]];
  lv_label_set_text(taskTitleLabel_, task.title);
  lv_label_set_text(taskTimeLabel_,
                    task.kind == TaskKind::Meeting ? "会议，已安排提醒" : "待办，已安排提醒");
}

void DeviceUi::refreshTasks() {
  if (!taskListLabel_) return;
  uint8_t indices[kMaxTasks];
  const uint8_t count = taskService_.sortedPendingIndices(indices, kMaxTasks);
  char text[320]{};
  if (!count) {
    strcpy(text, "所有事项都完成啦！");
  } else {
    for (uint8_t row = 0; row < count && row < 5; ++row) {
      const Task &task = tasks_.items[indices[row]];
      const size_t used = strlen(text);
      snprintf(text + used, sizeof(text) - used, "%u. %s\n", row + 1, task.title);
    }
  }
  lv_label_set_text(taskListLabel_, text);
}

void DeviceUi::begin(const lv_font_t *bodyFont, const lv_font_t *titleFont,
                     const lv_font_t *smallFont) {
  font_ = bodyFont;
  titleFont_ = titleFont;
  smallFont_ = smallFont;
  page_ = UiPage::Splash;
  pageStartedMs_ = millis();
  lastInteractionMs_ = pageStartedMs_;
  buildSplash();
  lv_obj_set_user_data(screen_, this);
  lv_scr_load(screen_);
}

void DeviceUi::show(UiPage page) {
  if (page == page_) return;
  const UiPage previousPage = page_;
  lv_group_remove_all_objs(lv_group_get_default());
  page_ = page;
  timeLabel_ = nullptr;
  networkLabel_ = nullptr;
  networkStatusDot_ = nullptr;
  taskTitleLabel_ = nullptr;
  taskTimeLabel_ = nullptr;
  taskListLabel_ = nullptr;
  weatherScene_ = nullptr;
  weatherCityLabel_ = nullptr;
  weatherTemperatureLabel_ = nullptr;
  weatherConditionLabel_ = nullptr;
  weatherHumidityLabel_ = nullptr;
  weatherAqiLabel_ = nullptr;
  weatherWindLabel_ = nullptr;
  weatherSourceLabel_ = nullptr;
  weatherDateLabel_ = nullptr;
  for (lv_obj_t *&icon : forecastIcon_) icon = nullptr;
  for (lv_obj_t *&label : forecastDateLabel_) label = nullptr;
  for (lv_obj_t *&label : forecastTemperatureLabel_) label = nullptr;
  detailLabel_ = nullptr;
  actionDialog_ = nullptr;
  confirmAction_ = UiAction::None;
  mascotImage_ = nullptr;
  gameStatusLabel_ = nullptr;
  for (lv_obj_t *&card : menuCards_) card = nullptr;
  menuHintLabel_ = nullptr;
  for (lv_obj_t *&cell : gameCells_) cell = nullptr;
  for (lv_obj_t *&digit : clockDigits_) digit = nullptr;
  for (auto &segments : clockSegments_) for (lv_obj_t *&segment : segments) segment = nullptr;
  for (lv_obj_t *&dot : clockColon_) dot = nullptr;
  clockSyncLabel_ = nullptr;
  for (lv_obj_t *&card : standbyDigitCards_) card = nullptr;
  for (lv_obj_t *&label : standbyDigitLabels_) label = nullptr;
  standbyColon_ = nullptr;
  for (lv_obj_t *&cell : memoryCells_) cell = nullptr;
  for (lv_obj_t *&label : memoryLabels_) label = nullptr;
  breakoutBoard_ = nullptr;
  breakoutPaddle_ = nullptr;
  breakoutBall_ = nullptr;
  for (lv_obj_t *&brick : breakoutBricks_) brick = nullptr;
  tetrisBoard_ = nullptr;
  pageStartedMs_ = millis();
  if (page == UiPage::Splash) buildSplash();
  else if (page == UiPage::Standby) buildStandby();
  else if (page == UiPage::Home) buildHome();
  else if (page == UiPage::Menu) buildMenu();
  else buildDetail(page);
  lv_obj_set_user_data(screen_, this);
  lv_scr_load_anim_t animation = LV_SCR_LOAD_ANIM_OVER_LEFT;
  uint16_t duration = 190;
  if (page == UiPage::Splash || page == UiPage::Standby || previousPage == UiPage::Splash ||
      previousPage == UiPage::Standby) {
    animation = LV_SCR_LOAD_ANIM_FADE_ON;
    duration = 170;
  } else if (page == UiPage::Home ||
             (page == UiPage::Menu && previousPage != UiPage::Home)) {
    animation = LV_SCR_LOAD_ANIM_MOVE_RIGHT;
  }
  pageTransitionUntilMs_ = millis() + duration + 35;
  lv_scr_load_anim(screen_, animation, duration, 0, true);
}

void DeviceUi::update(const char *timeText, const char *networkText, const char *ipText,
                      const char *networkDetails, const char *dateText) {
  const uint32_t now = millis();
  if (page_ == UiPage::Splash && now - pageStartedMs_ >= kSplashDurationMs) {
    show(UiPage::Home);
    return;
  }
  if (kAutomaticStandbyEnabled && page_ != UiPage::Splash && page_ != UiPage::Standby &&
      !networkBusy_ &&
      now - lastInteractionMs_ >= kStandbyDelayMs) {
    show(UiPage::Standby);
  }
  if ((page_ == UiPage::Splash || page_ == UiPage::Standby || page_ == UiPage::Home) &&
      mascotImage_) {
    const int16_t bob = ((now / 650) & 1U) ? 2 : 0;
    lv_obj_set_y(mascotImage_, mascotBaseY_ + bob);
  }
  if (page_ == UiPage::Clock) refreshClock(timeText);
  if (page_ == UiPage::Standby) refreshStandbyClock(timeText);
  if (page_ == UiPage::GameMemory && memoryHideAtMs_ &&
      static_cast<int32_t>(now - memoryHideAtMs_) >= 0) {
    memoryVisible_ = 0;
    memoryFirst_ = -1;
    memoryHideAtMs_ = 0;
    refreshMemoryGame();
  }
  snprintf(networkSummary_, sizeof(networkSummary_), "%s", networkDetails && networkDetails[0]
                                                           ? networkDetails : ipText);
  networkBusy_ = networkText &&
                 (strcmp(networkText, "连接中") == 0 || strcmp(networkText, "开启中") == 0);
  if (timeLabel_) lv_label_set_text(timeLabel_, timeText);
  if (weatherDateLabel_ && dateText) lv_label_set_text(weatherDateLabel_, dateText);
  refreshNetworkBadge(networkText);
  if (page_ == UiPage::Network && detailLabel_ &&
      strcmp(lv_label_get_text(detailLabel_), networkSummary_) != 0) {
    lv_label_set_text(detailLabel_, networkSummary_);
  }
}

bool DeviceUi::pollShortcuts(bool muted) {
  muted_ = muted;
  const bool a = digitalRead(WIO_KEY_A) == LOW;
  const bool b = digitalRead(WIO_KEY_B) == LOW;
  const bool c = digitalRead(WIO_KEY_C) == LOW;
  const bool up = digitalRead(WIO_5S_UP) == LOW;
  const bool down = digitalRead(WIO_5S_DOWN) == LOW;
  const bool left = digitalRead(WIO_5S_LEFT) == LOW;
  const bool right = digitalRead(WIO_5S_RIGHT) == LOW;
  const bool press = digitalRead(WIO_5S_PRESS) == LOW;
  const bool navigation = up || down || left || right || press;
  const bool anyKey = a || b || c || navigation;
  if (anyKey) lastInteractionMs_ = millis();
  if (static_cast<int32_t>(millis() - pageTransitionUntilMs_) < 0) {
    previousA_ = a;
    previousB_ = b;
    previousC_ = c;
    previousUp_ = up;
    previousDown_ = down;
    previousLeft_ = left;
    previousRight_ = right;
    previousPress_ = press;
    return false;
  }
  if (anyKey && (page_ == UiPage::Splash || page_ == UiPage::Standby)) {
    previousA_ = a;
    previousB_ = b;
    previousC_ = c;
    previousUp_ = up;
    previousDown_ = down;
    previousLeft_ = left;
    previousRight_ = right;
    previousPress_ = press;
    show(UiPage::Home);
    return false;
  }
  const uint32_t now = millis();
  if (page_ == UiPage::GameBreakout) {
    if (left && !previousLeft_) breakoutPaddleX_ = breakoutPaddleX_ > 14 ? breakoutPaddleX_ - 14 : 1;
    if (right && !previousRight_) breakoutPaddleX_ = breakoutPaddleX_ < 154 ? breakoutPaddleX_ + 14 : 168;
    if (!breakoutRunning_) breakoutBallX_ = breakoutPaddleX_ + 22;
    if (press && !previousPress_) {
      if (!breakoutBricksAlive_) resetBreakout();
      breakoutRunning_ = true;
      breakoutLastTickMs_ = now;
    }
    refreshBreakout();
    updateBreakout(now);
  }
  if (page_ == UiPage::GameTetris) {
    if (tetrisGameOver_ && press && !previousPress_) resetTetris();
    if (!tetrisGameOver_) {
      if (left && !previousLeft_ && tetrisCanPlace(tetrisX_ - 1, tetrisY_, tetrisRotation_)) --tetrisX_;
      if (right && !previousRight_ && tetrisCanPlace(tetrisX_ + 1, tetrisY_, tetrisRotation_)) ++tetrisX_;
      const uint8_t rotated = (tetrisRotation_ + 1U) & 3U;
      if (up && !previousUp_ && tetrisCanPlace(tetrisX_, tetrisY_, rotated)) tetrisRotation_ = rotated;
      if (down && !previousDown_) {
        if (tetrisCanPlace(tetrisX_, tetrisY_ + 1, tetrisRotation_)) ++tetrisY_;
        else lockTetrisPiece();
      }
      if (press && !previousPress_) {
        while (tetrisCanPlace(tetrisX_, tetrisY_ + 1, tetrisRotation_)) ++tetrisY_;
        lockTetrisPiece();
      }
      refreshTetris();
    }
    updateTetris(now);
  }
  if (page_ == UiPage::Menu) {
    uint8_t row = menuFocus_ / 3;
    uint8_t column = menuFocus_ % 3;
    if (up && !previousUp_ && row > 0) --row;
    if (down && !previousDown_ && row < 1) ++row;
    if (left && !previousLeft_ && column > 0) --column;
    if (right && !previousRight_ && column < 2) ++column;
    const uint8_t next = row * 3 + column;
    if (next != menuFocus_) {
      menuFocus_ = next;
      lv_group_focus_obj(menuCards_[menuFocus_]);
      refreshMenuFocus();
    }
    if (press && !previousPress_ && menuCards_[menuFocus_]) {
      lv_event_send(menuCards_[menuFocus_], LV_EVENT_CLICKED, nullptr);
    }
  }
  if (page_ == UiPage::Home) {
    if (left && !previousLeft_) showWeatherPreview(-1);
    if (right && !previousRight_) showWeatherPreview(1);
  }
  if (c && !previousC_) show(UiPage::Home);
  if (b && !previousB_) show(UiPage::Menu);
  if (a && !previousA_ && page_ != UiPage::Home) show(UiPage::Menu);
  const bool muteRequested = a && !previousA_ && page_ == UiPage::Home;
  previousA_ = a;
  previousB_ = b;
  previousC_ = c;
  previousUp_ = up;
  previousDown_ = down;
  previousLeft_ = left;
  previousRight_ = right;
  previousPress_ = press;
  return muteRequested;
}

}  // namespace wio_memo
