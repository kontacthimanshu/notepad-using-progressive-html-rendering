#pragma once
// Canonical headless single-line renderer (research R-21). The only producer of single-line images
// for pixelcss-tool, glyph goldens and canvas tests, so their outputs are byte-comparable.

#include <string_view>

#include "pxc/core/geometry.hpp"
#include "pxc/fonts/font_registry.hpp"
#include "pxc/fonts/raster_cache.hpp"
#include "pxc/render/framebuffer.hpp"

namespace pxc::fonts {

// Canvas: width = (Σadvance + 2)·s (+ −bearingX·s of the first glyph when negative),
// height = lineHeight·s filled with bg, pen starts at x = s, baselineY = ascent·s.
render::Framebuffer renderTextLine(const FontRegistry& registry, RasterCache& cache, std::string_view packId,
                                   std::u32string_view text, int scale, core::Color fg, core::Color bg);

} // namespace pxc::fonts
