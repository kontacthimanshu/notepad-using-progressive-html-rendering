#pragma once

#include <string>

#include "pxc/fonts/font_registry.hpp"

namespace pxc::fonts {

// Hollow box: width = max(3, spaceAdvance), height = ascent, baseline = height, 1-cell currentcolor border,
// backgroundcolor interior, advance = width + 1.
glyphcss::GlyphDefinition makeEngineReplacement(const std::string& family, const FontMetrics& m);

} // namespace pxc::fonts
