#include "pxc/fonts/text_line.hpp"

#include <algorithm>

#include "pxc/render/renderer.hpp"

namespace pxc::fonts {

render::Framebuffer renderTextLine(const FontRegistry& registry, RasterCache& cache, std::string_view packId,
                                   std::u32string_view text, int scale, core::Color fg, core::Color bg) {
    scale = std::max(1, scale);
    const FontMetrics fm = registry.metrics(packId);
    int sumAdvance = 0;
    int leadExtra = 0;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const ResolvedGlyph rg = registry.resolve(packId, text[i]);
        if (!rg.def) continue;
        sumAdvance += rg.def->advance;
        if (i == 0 && rg.def->bearingX < 0) leadExtra = -rg.def->bearingX;
    }
    const int width = (sumAdvance + 2 + leadExtra) * scale;
    const int height = std::max(1, fm.lineHeight) * scale;
    render::Framebuffer fb(width, height, bg);
    render::Renderer renderer;
    int pen = (1 + leadExtra) * scale;
    const int baseline = fm.ascent * scale;
    for (char32_t cp : text) {
        const ResolvedGlyph rg = registry.resolve(packId, cp);
        if (!rg.def) continue;
        const render::RasterGlyph& g = cache.get(packId, cp, scale, fg, bg);
        renderer.blitGlyph(fb, g, pen, baseline, fb.bounds());
        pen += rg.def->advance * scale;
    }
    return fb;
}

} // namespace pxc::fonts
