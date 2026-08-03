#include "wio_memo/application/clock.h"

namespace wio_memo {

uint32_t ClockPolicy::toLocalEpoch(uint32_t utc, int16_t offsetMinutes) {
  const int64_t adjusted = static_cast<int64_t>(utc) + static_cast<int32_t>(offsetMinutes) * 60L;
  if (adjusted <= 0) return 0;
  if (adjusted > 0xFFFFFFFFLL) return 0xFFFFFFFFUL;
  return static_cast<uint32_t>(adjusted);
}

}  // namespace wio_memo
