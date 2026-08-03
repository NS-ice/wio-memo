#pragma once

#include <stddef.h>
#include <stdint.h>

namespace wio_memo {

uint32_t crc32(const void *data, size_t size, uint32_t initial = 0xFFFFFFFFUL);

}  // namespace wio_memo
