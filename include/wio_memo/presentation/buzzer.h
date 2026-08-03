#pragma once

#include <stdint.h>

#include "wio_memo/application/reminder_engine.h"

namespace wio_memo {

class Buzzer {
 public:
  explicit Buzzer(int pin) : pin_(pin) {}
  void begin();
  void play(AlertType type, uint32_t nowMs);
  void update(uint32_t nowMs);
  void setMuted(bool muted);
  bool muted() const { return muted_; }
  bool active() const { return active_; }

 private:
  void stop();
  int pin_;
  bool muted_ = false;
  bool active_ = false;
  bool toneOn_ = false;
  uint8_t step_ = 0;
  uint8_t stepCount_ = 0;
  uint8_t duty_ = 64;
  uint32_t deadlineMs_ = 0;
  const uint16_t *durations_ = nullptr;
};

}  // namespace wio_memo
