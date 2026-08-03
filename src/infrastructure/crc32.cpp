#include "wio_memo/infrastructure/crc32.h"

namespace wio_memo {

uint32_t crc32(const void *data, size_t size, uint32_t initial) {
  const uint8_t *bytes = static_cast<const uint8_t *>(data);
  uint32_t value = initial;
  for (size_t i = 0; i < size; ++i) {
    value ^= bytes[i];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      value = (value >> 1) ^ (0xEDB88320UL & (0UL - (value & 1UL)));
    }
  }
  return value ^ 0xFFFFFFFFUL;
}

}  // namespace wio_memo
