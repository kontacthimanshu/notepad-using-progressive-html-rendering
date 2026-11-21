#pragma once
// Paints a PreviewLayout with the shared glyph pipeline (FontRegistry + RasterCache + Renderer, FR-037).

#include <string>

#include "pxc/fonts/raster_cache.hpp"
#include "pxc/layout/block_layout.hpp"
#include "pxc/render/framebuffer.hpp"
#include "pxc/render/renderer.hpp"
#include "pxc/update/update_manager.hpp"

namespace pxc::update {

struct PreviewPaintState {
    int scrollY = 0;
    std::size_t selStart = 0;
    std::size_t selEnd = 0;
    std::u32string banner; // non-empty: yellow diagnostics banner drawn at the top with pack glyphs
    std::string bannerPack = "verdana-style";
    int scale = 1;
};

void paintPreview(render::Framebuffer& fb, core::Rect clip, const layout::PreviewLayout& layout,
                  const PreviewPaintState& state, const fonts::FontRegistry& registry, fonts::RasterCache& cache,
                  render::Renderer& renderer, const Theme& theme);

// Height of the banner row in device pixels (0 when there is no banner).
int previewBannerHeight(const PreviewPaintState& state, const fonts::FontRegistry& registry);

} // namespace pxc::update
