#include "pxc/render/renderer.hpp"

namespace pxc::render {

void Renderer::fill(Framebuffer& fb, Rect r, Color c) {
    const Rect clip = r.intersect(fb.bounds());
    fb.fillRect(clip, c);
    counters_.pixelsWritten += static_cast<std::uint64_t>(clip.area());
}

void Renderer::blitGlyph(Framebuffer& fb, const RasterGlyph& g, int penX, int baselineY, Rect clip) {
    if (g.width <= 0 || g.height <= 0) return;
    const int left = penX + g.originX;
    const int top = baselineY + g.originY;
    const Rect dst = Rect{left, top, g.width, g.height}.intersect(clip).intersect(fb.bounds());
    if (dst.empty()) return;
    ++counters_.glyphBlits;
    for (int y = dst.y; y < dst.bottom(); ++y) {
        auto row = fb.row(y);
        const std::uint32_t* src = g.pixels.data() + static_cast<std::size_t>(y - top) * static_cast<std::size_t>(g.width);
        for (int x = dst.x; x < dst.right(); ++x) {
            const std::uint32_t s = src[x - left];
            if ((s >> 24) == 0) continue; // transparent cell never overwrites the background
            std::uint32_t& d = row[static_cast<std::size_t>(x)];
            d = blendPixel(d, s);
            ++counters_.pixelsWritten;
        }
    }
}

void Renderer::drawCaret(Framebuffer& fb, Rect r, Color c) { fill(fb, r, c); }

} // namespace pxc::render
