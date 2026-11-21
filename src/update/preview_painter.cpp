#include "pxc/update/preview_painter.hpp"

#include <algorithm>

namespace pxc::update {

namespace {
const core::Color kBanner{0xFF, 0xF3, 0xB0, 255};
} // namespace

int previewBannerHeight(const PreviewPaintState& state, const fonts::FontRegistry& registry) {
    if (state.banner.empty()) return 0;
    return (registry.metrics(state.bannerPack).lineHeight + 4) * std::max(1, state.scale);
}

void paintPreview(render::Framebuffer& fb, core::Rect clip, const layout::PreviewLayout& lay,
                  const PreviewPaintState& state, const fonts::FontRegistry& registry, fonts::RasterCache& cache,
                  render::Renderer& renderer, const Theme& theme) {
    clip = clip.intersect(fb.bounds());
    if (clip.empty()) return;
    const int s = std::max(1, state.scale);
    const int top = previewBannerHeight(state, registry) - state.scrollY;
    renderer.fill(fb, clip, theme.background);

    // Block and inline backgrounds.
    for (const auto& bgr : lay.backgrounds) {
        const core::Rect r{bgr.rect.x, bgr.rect.y + top, bgr.rect.w, bgr.rect.h};
        renderer.fill(fb, r.intersect(clip), bgr.color);
    }

    const bool hasSel = state.selEnd > state.selStart;
    for (const auto& row : lay.rows) {
        const int ry = row.y + top;
        const core::Rect rowRect{0, ry, fb.width(), row.height};
        const core::Rect rowClip = rowRect.intersect(clip);
        if (rowClip.empty()) continue;
        for (const auto& g : row.glyphs) {
            const bool sel = hasSel && g.offset >= state.selStart && g.offset < state.selEnd;
            const core::Color bg = sel ? theme.selectionFill : g.bg;
            if (sel) renderer.fill(fb, core::Rect{g.x, ry, g.advance, row.height}.intersect(rowClip), bg);
            if (g.cp == U' ' || g.cp == U'\t') continue;
            const core::Color fg = sel ? theme.selectionText : g.fg;
            const auto& raster = cache.get(lay.packs[g.pack], g.cp, s, fg, bg);
            renderer.blitGlyph(fb, raster, g.x, ry + row.baseline, rowClip);
        }
    }

    // Diagnostics banner, drawn with pack glyphs.
    if (!state.banner.empty()) {
        const int h = previewBannerHeight(state, registry);
        const core::Rect band = core::Rect{0, 0, fb.width(), h}.intersect(clip);
        renderer.fill(fb, band, kBanner);
        const auto fm = registry.metrics(state.bannerPack);
        int pen = 6 * s;
        const int baseline = 2 * s + fm.ascent * s;
        for (char32_t cp : state.banner) {
            const auto rg = registry.resolve(state.bannerPack, cp);
            if (!rg.def) continue;
            if (cp != U' ') renderer.blitGlyph(fb, cache.get(state.bannerPack, cp, s, theme.text, kBanner), pen, baseline, band);
            pen += rg.def->advance * s;
        }
    }
}

} // namespace pxc::update
