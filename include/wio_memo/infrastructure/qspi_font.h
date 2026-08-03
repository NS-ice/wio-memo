#pragma once

#include <stddef.h>
#include <stdint.h>
#include <sfud.h>

namespace wio_memo {

constexpr uint32_t kQspiFont16Address = 0x000000;
constexpr uint32_t kQspiFont12Address = 0x050000;
constexpr uint32_t kQspiFont20Address = 0x080000;
constexpr uint16_t kFontFormatVersion = 1;
constexpr size_t kMaxGlyphBitmapBytes = 72;  // 24x24 at 1 bit per pixel

#pragma pack(push, 1)
struct FontHeader {
  char magic[4];             // WMF1
  uint16_t version;
  uint8_t glyphWidth;
  uint8_t glyphHeight;
  uint32_t glyphCount;
  uint32_t indexOffset;
  uint32_t dataOffset;
  uint16_t bytesPerGlyph;
  uint16_t reserved;
  uint32_t payloadCrc32;
  uint32_t headerCrc32;
};

struct GlyphRecord {
  uint32_t codepoint;
  uint32_t bitmapOffset;
};
#pragma pack(pop)

struct GlyphBitmap {
  uint8_t width = 0;
  uint8_t height = 0;
  uint16_t byteCount = 0;
  uint8_t data[kMaxGlyphBitmapBytes]{};
};

class QspiFont {
 public:
  bool begin(uint32_t baseAddress = kQspiFont16Address);
  bool readGlyph(uint32_t codepoint, GlyphBitmap &bitmap) const;
  bool available() const { return flash_ != nullptr; }
  uint32_t glyphCount() const { return header_.glyphCount; }
  uint8_t glyphWidth() const { return header_.glyphWidth; }
  uint8_t glyphHeight() const { return header_.glyphHeight; }

 private:
  bool read(uint32_t relativeAddress, void *destination, size_t size) const;
  const sfud_flash *flash_ = nullptr;
  uint32_t baseAddress_ = 0;
  FontHeader header_{};
};

static_assert(sizeof(FontHeader) == 32, "WMF header layout changed");
static_assert(sizeof(GlyphRecord) == 8, "WMF index layout changed");

}  // namespace wio_memo
