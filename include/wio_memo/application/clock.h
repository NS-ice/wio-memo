#pragma once

#include <stdint.h>

#include "wio_memo/domain/task.h"

namespace wio_memo {

class ClockPolicy {
 public:
  static bool isValidUtc(uint32_t utc) { return utc >= kMinimumValidEpoch; }
  static uint32_t toLocalEpoch(uint32_t utc, int16_t offsetMinutes);
  static bool intervalElapsed(uint32_t nowMs, uint32_t previousMs, uint32_t intervalMs) {
    return static_cast<uint32_t>(nowMs - previousMs) >= intervalMs;
  }
};

}  // namespace wio_memo
