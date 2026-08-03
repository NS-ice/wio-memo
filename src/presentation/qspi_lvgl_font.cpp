#include "wio_memo/presentation/qspi_lvgl_font.h"

namespace wio_memo {

bool QspiLvglFont::load(uint32_t codepoint) {
  if (!source_ || codepoint < 0x80) return false;
  if (cachedCodepoint_ == codepoint) return true;
  if (!source_->readGlyph(codepoint, cachedGlyph_)) return false;
  cachedCodepoint_ = codepoint;
  return true;
}

bool QspiLvglFont::describeGlyph(const lv_font_t *font, lv_font_glyph_dsc_t *description,
                                 uint32_t codepoint, uint32_t nextCodepoint) {
  (void)nextCodepoint;
  QspiLvglFont *self = static_cast<QspiLvglFont *>(const_cast<void *>(font->dsc));
  if (!self->load(codepoint)) return false;
  // LVGL 8 uses pixel units for adv_w. Shifting by four made every 16 px
  // Chinese glyph advance by 256 px, which scattered label characters across
  // the whole display.
  description->adv_w = self->cachedGlyph_.width;
  description->box_w = self->cachedGlyph_.width;
  description->box_h = self->cachedGlyph_.height;
  description->ofs_x = 0;
  description->ofs_y = 0;
  description->bpp = 1;
  description->is_placeholder = false;
  return true;
}

const uint8_t *QspiLvglFont::glyphBitmap(const lv_font_t *font, uint32_t codepoint) {
  QspiLvglFont *self = static_cast<QspiLvglFont *>(const_cast<void *>(font->dsc));
  return self->load(codepoint) ? self->cachedGlyph_.data : nullptr;
}

void QspiLvglFont::begin(QspiFont &source, const lv_font_t *fallback) {
  source_ = &source;
  cachedCodepoint_ = 0;
  font_ = lv_font_t{};
  font_.get_glyph_dsc = describeGlyph;
  font_.get_glyph_bitmap = glyphBitmap;
  font_.line_height = source.glyphHeight();
  font_.base_line = 0;
  font_.subpx = LV_FONT_SUBPX_NONE;
  font_.fallback = fallback;
  font_.dsc = this;
}

}  // namespace wio_memo
