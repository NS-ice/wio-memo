#pragma once

#include <Arduino.h>
#include <lvgl.h>

class TFT_eSPI;

namespace wio_memo {

class LvglPort {
 public:
  void begin(TFT_eSPI &display);
  void update(uint32_t nowMs);
  void setKeypadEnabled(bool enabled);
  lv_group_t *inputGroup() const { return inputGroup_; }

 private:
  static constexpr uint16_t kWidth = 320;
  // Seeed's official Wio Terminal LVGL port uses a 10-row partial buffer.
  static constexpr uint16_t kBufferRows = 10;

  static void flush(lv_disp_drv_t *driver, const lv_area_t *area, lv_color_t *pixels);
  static void readKeypad(lv_indev_drv_t *driver, lv_indev_data_t *data);
  static uint32_t readHardwareKey();

  static TFT_eSPI *display_;
  static lv_color_t bufferA_[kWidth * kBufferRows];
  static uint32_t lastKey_;
  static bool keypadEnabled_;
  static bool keypadReleaseRequired_;

  lv_disp_draw_buf_t drawBuffer_{};
  lv_disp_drv_t displayDriver_{};
  lv_indev_drv_t inputDriver_{};
  lv_group_t *inputGroup_ = nullptr;
  uint32_t lastTickMs_ = 0;
};

}  // namespace wio_memo
