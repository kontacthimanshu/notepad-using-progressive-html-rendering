#pragma once
// GlyphDefinition -> RasterGlyph conversion (nearest-neighbour integer scaling).

#include "pxc/core/geometry.hpp"
#include "pxc/glyphcss/glyph_definition.hpp"
#include "pxc/render/raster_glyph.hpp"

namespace pxc::fonts {

// currentcolor -> fg, backgroundcolor -> bg (both opaque), literals keep their RGBA,
// transparent cells keep alpha 0. The background is never pre-composited.
render::RasterGlyph rasterize(const glyphcss::GlyphDefinition& g, int scale, core::Color fg, core::Color bg);

} // namespace pxc::fonts
