#include "pxc/fonts/rasterize.hpp"

#include <algorithm>

namespace pxc::fonts {

render::RasterGlyph rasterize(const glyphcss::GlyphDefinition& g, int scale, core::Color fg, core::Color bg) {
    using glyphcss::CellKind;
    scale = std::max(1, scale);
    render::RasterGlyph r;
    r.width = g.width * scale;
    r.height = g.height * scale;
    r.originX = g.bearingX * scale;
    r.originY = -g.baseline * scale;
    r.pixels.assign(static_cast<std::size_t>(r.width) * static_cast<std::size_t>(r.height), 0u);
    if (g.cells.size() != static_cast<std::size_t>(g.width) * static_cast<std::size_t>(g.height)) return r;

    core::Color fgo = fg;
    fgo.a = 255;
    core::Color bgo = bg;
    bgo.a = 255;
    for (int row = 0; row < g.height; ++row) {
        for (int col = 0; col < g.width; ++col) {
            const auto& c = g.cellAt(col, row);
            std::uint32_t v = 0;
            switch (c.kind) {
            case CellKind::Transparent: continue;
            case CellKind::Literal: v = c.rgba.toBGRA(); break;
            case CellKind::CurrentColor: v = fgo.toBGRA(); break;
            case CellKind::BackgroundColor: v = bgo.toBGRA(); break;
            }
            for (int dy = 0; dy < scale; ++dy) {
                std::uint32_t* dst = r.pixels.data() +
                                     static_cast<std::size_t>(row * scale + dy) * static_cast<std::size_t>(r.width) +
                                     static_cast<std::size_t>(col * scale);
                std::fill(dst, dst + scale, v);
            }
        }
    }
    return r;
}

} // namespace pxc::fonts
