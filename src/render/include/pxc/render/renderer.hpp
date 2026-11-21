#pragma once
// Pixel composition into the software framebuffer: fills, glyph blits (integer source-over), caret.

#include <cstdint>

#include "pxc/render/framebuffer.hpp"
#include "pxc/render/raster_glyph.hpp"

namespace pxc::render {

struct RendererCounters {
    std::uint64_t glyphBlits = 0;
    std::uint64_t pixelsWritten = 0;
    void reset() noexcept { *this = {}; }
};

// out = (s*a + d*(255-a) + 127) / 255 per colour channel; alpha composes the same way.
constexpr std::uint32_t blendPixel(std::uint32_t dst, std::uint32_t src) noexcept {
    const std::uint32_t a = src >> 24;
    if (a == 0) return dst;
    if (a == 255) return src;
    const std::uint32_t ia = 255 - a;
    auto ch = [&](int shift) {
        const std::uint32_t s = (src >> shift) & 0xFF, d = (dst >> shift) & 0xFF;
        return ((s * a + d * ia + 127) / 255) << shift;
    };
    const std::uint32_t da = dst >> 24;
    const std::uint32_t outA = (a * 255 + da * ia + 127) / 255;
    return (outA << 24) | ch(16) | ch(8) | ch(0);
}

class Renderer {
public:
    void fill(Framebuffer& fb, Rect r, Color c);
    // Blits `g` with its origin relative to (penX, baselineY), clipped to `clip` ∩ framebuffer.
    void blitGlyph(Framebuffer& fb, const RasterGlyph& g, int penX, int baselineY, Rect clip);
    void drawCaret(Framebuffer& fb, Rect r, Color c);

    RendererCounters& counters() noexcept { return counters_; }
    const RendererCounters& counters() const noexcept { return counters_; }

private:
    RendererCounters counters_;
};

} // namespace pxc::render
