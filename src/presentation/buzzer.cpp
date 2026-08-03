#include "wio_memo/presentation/buzzer.h"

#include <Arduino.h>

namespace wio_memo {
namespace {

const uint16_t kAdvancePattern[] = {90, 90, 90};
const uint16_t kStartPattern[] = {130, 80, 130};
const uint16_t kSnoozePattern[] = {70, 70, 70};

}  // namespace

void Buzzer::begin() {
  pinMode(pin_, OUTPUT);
  analogWrite(pin_, 0);
}

void Buzzer::play(AlertType type, uint32_t nowMs) {
  if (muted_ || type == AlertType::None) return;
  switch (type) {
    case AlertType::Advance:
      durations_ = kAdvancePattern;
      stepCount_ = sizeof(kAdvancePattern) / sizeof(kAdvancePattern[0]);
      break;
    case AlertType::Start:
      durations_ = kStartPattern;
      stepCount_ = sizeof(kStartPattern) / sizeof(kStartPattern[0]);
      break;
    case AlertType::Snooze:
      durations_ = kSnoozePattern;
      stepCount_ = sizeof(kSnoozePattern) / sizeof(kSnoozePattern[0]);
      break;
    default:
      return;
  }
  step_ = 0;
  toneOn_ = true;
  active_ = true;
  analogWrite(pin_, duty_);
  deadlineMs_ = nowMs + durations_[0];
}

void Buzzer::update(uint32_t nowMs) {
  if (!active_ || static_cast<int32_t>(nowMs - deadlineMs_) < 0) return;
  ++step_;
  if (step_ >= stepCount_) {
    stop();
    return;
  }
  toneOn_ = !toneOn_;
  analogWrite(pin_, toneOn_ ? duty_ : 0);
  deadlineMs_ = nowMs + durations_[step_];
}

void Buzzer::setMuted(bool muted) {
  muted_ = muted;
  if (muted_) stop();
}

void Buzzer::stop() {
  analogWrite(pin_, 0);
  active_ = false;
  toneOn_ = false;
  step_ = 0;
}

}  // namespace wio_memo
