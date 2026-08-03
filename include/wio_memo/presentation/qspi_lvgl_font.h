#pragma once

#include <lvgl.h>

#include "wio_memo/infrastructure/qspi_font.h"

namespace wio_memo {

class QspiLvglFont {
 public:
  void begin(QspiFont &source, const lv_font_t *fallback);
  const lv_font_t *font() const { return &font_; }

 private:
  static bool describeGlyph(const lv_font_t *font, lv_font_glyph_dsc_t *description,
                            uint32_t codepoint, uint32_t nextCodepoint);
  static const uint8_t *glyphBitmap(const lv_font_t *font, uint32_t codepoint);
  bool load(uint32_t codepoint);

  QspiFont *source_ = nullptr;
  GlyphBitmap cachedGlyph_{};
  uint32_t cachedCodepoint_ = 0;
  lv_font_t font_{};
};

}  // namespace wio_memo
