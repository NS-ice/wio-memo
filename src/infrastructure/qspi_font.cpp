#include "wio_memo/infrastructure/qspi_font.h"

#include <sfud.h>
#include <string.h>

#include "wio_memo/infrastructure/crc32.h"

namespace wio_memo {

bool QspiFont::read(uint32_t relativeAddress, void *destination, size_t size) const {
  return flash_ && sfud_read(flash_, baseAddress_ + relativeAddress, size,
                             static_cast<uint8_t *>(destination)) == SFUD_SUCCESS;
}

bool QspiFont::begin(uint32_t baseAddress) {
  flash_ = nullptr;
  baseAddress_ = baseAddress;
  memset(&header_, 0, sizeof(header_));
  if (sfud_init() != SFUD_SUCCESS) return false;
#ifdef SFUD_USING_QSPI
  sfud_qspi_fast_read_enable(sfud_get_device(SFUD_W25Q32_DEVICE_INDEX), 2);
#endif
  flash_ = sfud_get_device(SFUD_W25Q32_DEVICE_INDEX);
  if (!read(0, &header_, sizeof(header_))) {
    flash_ = nullptr;
    return false;
  }
  const uint32_t expectedHeaderCrc = crc32(&header_, offsetof(FontHeader, headerCrc32));
  const uint32_t indexBytes = header_.glyphCount * sizeof(GlyphRecord);
  const uint64_t dataEnd = static_cast<uint64_t>(header_.dataOffset) +
                           static_cast<uint64_t>(header_.glyphCount) * header_.bytesPerGlyph;
  const bool valid = memcmp(header_.magic, "WMF1", 4) == 0 &&
                     header_.version == kFontFormatVersion && header_.glyphWidth >= 8 &&
                     header_.glyphWidth <= 24 && header_.glyphHeight >= 8 &&
                     header_.glyphHeight <= 24 && header_.bytesPerGlyph > 0 &&
                     header_.bytesPerGlyph <= kMaxGlyphBitmapBytes &&
                     header_.indexOffset >= sizeof(FontHeader) &&
                     header_.dataOffset >= header_.indexOffset + indexBytes &&
                     dataEnd <= flash_->chip.capacity && header_.headerCrc32 == expectedHeaderCrc;
  if (!valid) flash_ = nullptr;
  return valid;
}

bool QspiFont::readGlyph(uint32_t codepoint, GlyphBitmap &bitmap) const {
  if (!available()) return false;
  uint32_t low = 0;
  uint32_t high = header_.glyphCount;
  while (low < high) {
    const uint32_t mid = low + (high - low) / 2;
    GlyphRecord record{};
    if (!read(header_.indexOffset + mid * sizeof(GlyphRecord), &record, sizeof(record))) return false;
    if (record.codepoint < codepoint) {
      low = mid + 1;
    } else if (record.codepoint > codepoint) {
      high = mid;
    } else {
      if (record.bitmapOffset + header_.bytesPerGlyph >
          header_.glyphCount * header_.bytesPerGlyph) {
        return false;
      }
      bitmap = GlyphBitmap{};
      bitmap.width = header_.glyphWidth;
      bitmap.height = header_.glyphHeight;
      bitmap.byteCount = header_.bytesPerGlyph;
      return read(header_.dataOffset + record.bitmapOffset, bitmap.data, bitmap.byteCount);
    }
  }
  return false;
}

}  // namespace wio_memo
