#pragma once

#include <stdint.h>

#include "wio_memo/infrastructure/qspi_font.h"

class TFT_eSPI;

namespace wio_memo {

class Utf8Renderer {
 public:
  Utf8Renderer(TFT_eSPI &display, QspiFont &font) : display_(display), font_(font) {}
  int16_t draw(const char *text, int16_t x, int16_t y, int16_t maxWidth, uint16_t foreground,
               uint16_t background);
  static bool nextCodepoint(const char *&cursor, uint32_t &codepoint);

 private:
  void drawGlyph(const GlyphBitmap &glyph, int16_t x, int16_t y, uint16_t foreground,
                 uint16_t background);
  TFT_eSPI &display_;
  QspiFont &font_;
};

}  // namespace wio_memo
