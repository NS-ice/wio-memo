#include "wio_memo/presentation/utf8_renderer.h"

#include <TFT_eSPI.h>

namespace wio_memo {

bool Utf8Renderer::nextCodepoint(const char *&cursor, uint32_t &codepoint) {
  const uint8_t first = static_cast<uint8_t>(*cursor++);
  if (first == 0) return false;
  if (first < 0x80) {
    codepoint = first;
    return true;
  }
  uint8_t remaining = 0;
  if ((first & 0xE0) == 0xC0) {
    codepoint = first & 0x1F;
    remaining = 1;
  } else if ((first & 0xF0) == 0xE0) {
    codepoint = first & 0x0F;
    remaining = 2;
  } else if ((first & 0xF8) == 0xF0) {
    codepoint = first & 0x07;
    remaining = 3;
  } else {
    codepoint = 0xFFFD;
    return true;
  }
  for (uint8_t i = 0; i < remaining; ++i) {
    const uint8_t next = static_cast<uint8_t>(*cursor);
    if ((next & 0xC0) != 0x80) {
      codepoint = 0xFFFD;
      return true;
    }
    ++cursor;
    codepoint = (codepoint << 6) | (next & 0x3F);
  }
  return true;
}

void Utf8Renderer::drawGlyph(const GlyphBitmap &glyph, int16_t x, int16_t y, uint16_t foreground,
                             uint16_t background) {
  uint16_t bit = 0;
  for (uint8_t row = 0; row < glyph.height; ++row) {
    for (uint8_t column = 0; column < glyph.width; ++column, ++bit) {
      const bool set = (glyph.data[bit >> 3] & (0x80 >> (bit & 7))) != 0;
      display_.drawPixel(x + column, y + row, set ? foreground : background);
    }
  }
}

int16_t Utf8Renderer::draw(const char *text, int16_t x, int16_t y, int16_t maxWidth,
                           uint16_t foreground, uint16_t background) {
  const int16_t origin = x;
  const char *cursor = text;
  uint32_t codepoint = 0;
  while (nextCodepoint(cursor, codepoint)) {
    const int16_t width = codepoint < 128 ? 6 : (font_.available() ? font_.glyphWidth() : 12);
    if (x + width > origin + maxWidth) break;
    if (codepoint >= 32 && codepoint < 127) {
      display_.drawChar(x, y, static_cast<uint16_t>(codepoint), foreground, background, 1);
    } else {
      GlyphBitmap glyph{};
      if (font_.readGlyph(codepoint, glyph)) {
        drawGlyph(glyph, x, y, foreground, background);
      } else {
        display_.fillRect(x, y, width, font_.available() ? font_.glyphHeight() : 12, background);
        display_.drawRect(x + 1, y + 1, width - 2,
                          (font_.available() ? font_.glyphHeight() : 12) - 2, foreground);
      }
    }
    x += width;
  }
  return x - origin;
}

}  // namespace wio_memo
