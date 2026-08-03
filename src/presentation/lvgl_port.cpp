#include "wio_memo/presentation/lvgl_port.h"

#include <TFT_eSPI.h>

namespace wio_memo {

TFT_eSPI *LvglPort::display_ = nullptr;
lv_color_t LvglPort::bufferA_[kWidth * kBufferRows];
uint32_t LvglPort::lastKey_ = LV_KEY_ENTER;
bool LvglPort::keypadEnabled_ = true;
bool LvglPort::keypadReleaseRequired_ = false;

void LvglPort::flush(lv_disp_drv_t *driver, const lv_area_t *area, lv_color_t *pixels) {
  (void)driver;
  const uint16_t width = static_cast<uint16_t>(area->x2 - area->x1 + 1);
  const uint16_t height = static_cast<uint16_t>(area->y2 - area->y1 + 1);
  const uint32_t pixelCount = static_cast<uint32_t>(width) * height;
  display_->startWrite();
  display_->setAddrWindow(area->x1, area->y1, width, height);
  // Keep the proven synchronous path on Wio Terminal. Seeed's SAMD51
  // pushColors() ignores its byte-swap argument; mutating and bulk-transferring
  // LVGL's active buffer caused regressions on the physical device.
  for (uint32_t index = 0; index < pixelCount; ++index) {
    display_->writeColor(pixels[index].full, 1);
  }
  display_->endWrite();
  lv_disp_flush_ready(driver);
}

uint32_t LvglPort::readHardwareKey() {
  // A keypad-controlled LVGL group changes focus with PREV/NEXT. Plain
  // UP/DOWN/LEFT/RIGHT are delivered to the focused widget and do not move
  // between menu buttons.
  if (digitalRead(WIO_5S_UP) == LOW || digitalRead(WIO_5S_LEFT) == LOW) return LV_KEY_PREV;
  if (digitalRead(WIO_5S_DOWN) == LOW || digitalRead(WIO_5S_RIGHT) == LOW) return LV_KEY_NEXT;
  if (digitalRead(WIO_5S_PRESS) == LOW) return LV_KEY_ENTER;
  return 0;
}

void LvglPort::readKeypad(lv_indev_drv_t *driver, lv_indev_data_t *data) {
  (void)driver;
  if (keypadReleaseRequired_) {
    const bool released = digitalRead(WIO_5S_UP) != LOW && digitalRead(WIO_5S_DOWN) != LOW &&
                          digitalRead(WIO_5S_LEFT) != LOW && digitalRead(WIO_5S_RIGHT) != LOW &&
                          digitalRead(WIO_5S_PRESS) != LOW;
    if (released) keypadReleaseRequired_ = false;
  }
  if (!keypadEnabled_ || keypadReleaseRequired_) {
    data->key = lastKey_;
    data->state = LV_INDEV_STATE_RELEASED;
    return;
  }
  const uint32_t key = readHardwareKey();
  if (key) {
    lastKey_ = key;
    data->key = key;
    data->state = LV_INDEV_STATE_PRESSED;
  } else {
    data->key = lastKey_;
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

void LvglPort::setKeypadEnabled(bool enabled) {
  if (enabled && !keypadEnabled_) keypadReleaseRequired_ = true;
  keypadEnabled_ = enabled;
}

void LvglPort::begin(TFT_eSPI &display) {
  display_ = &display;
  display_->setSwapBytes(false);
  lv_init();
  lv_disp_draw_buf_init(&drawBuffer_, bufferA_, nullptr, kWidth * kBufferRows);

  lv_disp_drv_init(&displayDriver_);
  displayDriver_.hor_res = 320;
  displayDriver_.ver_res = 240;
  displayDriver_.flush_cb = flush;
  displayDriver_.draw_buf = &drawBuffer_;
  displayDriver_.full_refresh = 0;
  displayDriver_.direct_mode = 0;
  lv_disp_drv_register(&displayDriver_);

  lv_indev_drv_init(&inputDriver_);
  inputDriver_.type = LV_INDEV_TYPE_KEYPAD;
  inputDriver_.read_cb = readKeypad;
  lv_indev_t *input = lv_indev_drv_register(&inputDriver_);
  inputGroup_ = lv_group_create();
  lv_group_set_default(inputGroup_);
  lv_indev_set_group(input, inputGroup_);
  lastTickMs_ = millis();
}

void LvglPort::update(uint32_t nowMs) {
  const uint32_t elapsed = nowMs - lastTickMs_;
  if (elapsed) {
    lv_tick_inc(elapsed);
    lastTickMs_ = nowMs;
  }
  lv_timer_handler();
}

}  // namespace wio_memo
