// Engine replacement glyph (research R-07 step 3): a hollow box generated in code from pack metrics, so the
// final fallback step can never fail to load.
#include "engine_glyph.hpp"

#include <algorithm>

namespace pxc::fonts {

glyphcss::GlyphDefinition makeEngineReplacement(const std::string& family, const FontMetrics& m) {
    using glyphcss::Cell;
    using glyphcss::CellKind;
    glyphcss::GlyphDefinition g;
    g.schema = glyphcss::kGlyphSchema;
    g.codePoint = 0xFFFD;
    g.family = family;
    g.width = std::max(3, m.spaceAdvance);
    g.height = std::max(3, m.ascent);
    g.baseline = g.height; // sits on the baseline
    g.bearingX = 0;
    g.advance = g.width + 1;
    g.cells.resize(static_cast<std::size_t>(g.width) * static_cast<std::size_t>(g.height));
    for (int r = 0; r < g.height; ++r)
        for (int c = 0; c < g.width; ++c) {
            const bool border = r == 0 || c == 0 || r == g.height - 1 || c == g.width - 1;
            g.cells[static_cast<std::size_t>(r) * static_cast<std::size_t>(g.width) + static_cast<std::size_t>(c)] =
                Cell{border ? CellKind::CurrentColor : CellKind::BackgroundColor, {}};
        }
    // Stable identity for the raster cache: derived from the geometry, never from file contents.
    std::string id = "engine-replacement:" + family + ":" + std::to_string(g.width) + "x" + std::to_string(g.height);
    g.contentHash = glyphcss::contentHash(id);
    g.sourcePath = "<engine>";
    return g;
}

} // namespace pxc::fonts
