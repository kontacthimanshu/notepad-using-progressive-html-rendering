#include <doctest.h>

#include "pxc/render/renderer.hpp"

using namespace pxc;
using namespace pxc::render;

namespace {
std::uint32_t argb(std::uint32_t a, std::uint32_t r, std::uint32_t g, std::uint32_t b) {
    return (a << 24) | (r << 16) | (g << 8) | b;
}
RasterGlyph solid(int w, int h, std::uint32_t px, int ox = 0, int oy = 0) {
    RasterGlyph g;
    g.width = w;
    g.height = h;
    g.originX = ox;
    g.originY = oy;
    g.pixels.assign(static_cast<std::size_t>(w * h), px);
    return g;
}
} // namespace

TEST_CASE("source-over integer formula for specific alpha values") {
    const std::uint32_t dst = argb(255, 200, 100, 0);
    // a = 0: destination untouched
    CHECK(blendPixel(dst, argb(0, 10, 20, 30)) == dst);
    // a = 255: source replaces
    CHECK(blendPixel(dst, argb(255, 10, 20, 30)) == argb(255, 10, 20, 30));
    // a = 128: (s*128 + d*127 + 127) / 255
    auto ch = [](std::uint32_t s, std::uint32_t d, std::uint32_t a) { return (s * a + d * (255 - a) + 127) / 255; };
    CHECK(blendPixel(dst, argb(128, 0, 0, 255)) == argb(255, ch(0, 200, 128), ch(0, 100, 128), ch(255, 0, 128)));
    CHECK(blendPixel(dst, argb(1, 0, 0, 0)) == argb(255, ch(0, 200, 1), ch(0, 100, 1), 0));
    CHECK(blendPixel(dst, argb(254, 255, 255, 255)) ==
          argb(255, ch(255, 200, 254), ch(255, 100, 254), ch(255, 0, 254)));
}

TEST_CASE("transparent cells leave the destination untouched") {
    Framebuffer fb(4, 4, core::Color{9, 9, 9, 255});
    RasterGlyph g = solid(2, 2, 0u);
    g.pixels[0] = argb(255, 255, 0, 0);
    Renderer r;
    r.blitGlyph(fb, g, 1, 1, fb.bounds());
    CHECK(fb.color(1, 1) == core::Color{255, 0, 0, 255});
    CHECK(fb.color(2, 1) == core::Color{9, 9, 9, 255});
    CHECK(fb.color(1, 2) == core::Color{9, 9, 9, 255});
}

TEST_CASE("blitGlyph clips at all four framebuffer edges") {
    Framebuffer fb(4, 4, core::kWhite);
    Renderer r;
    const auto red = argb(255, 255, 0, 0);
    r.blitGlyph(fb, solid(3, 3, red), -1, -1, fb.bounds()); // top-left
    r.blitGlyph(fb, solid(3, 3, red), 3, 3, fb.bounds());   // bottom-right
    CHECK(fb.pixel(0, 0) == red);
    CHECK(fb.pixel(1, 1) == red);
    CHECK(fb.pixel(2, 2) == core::kWhite.toBGRA());
    CHECK(fb.pixel(3, 3) == red);
    r.blitGlyph(fb, solid(2, 2, red), 100, 100, fb.bounds()); // fully outside: no crash
}

TEST_CASE("blitGlyph honours an arbitrary clip rectangle and origin offsets") {
    Framebuffer fb(6, 6, core::kWhite);
    Renderer r;
    const auto blue = argb(255, 0, 0, 255);
    // origin (-1,-3) relative to pen (2,4): glyph spans x 1..4, y 1..4
    r.blitGlyph(fb, solid(4, 4, blue, -1, -3), 2, 4, core::Rect{2, 2, 2, 2});
    for (int y = 0; y < 6; ++y)
        for (int x = 0; x < 6; ++x) {
            const bool inside = x >= 2 && x < 4 && y >= 2 && y < 4;
            CAPTURE(x);
            CAPTURE(y);
            CHECK((fb.pixel(x, y) == blue) == inside);
        }
}

TEST_CASE("caret rectangle is drawn") {
    Framebuffer fb(5, 5, core::kWhite);
    Renderer r;
    r.drawCaret(fb, {2, 0, 1, 5}, core::kBlack);
    for (int y = 0; y < 5; ++y) {
        CHECK(fb.color(2, y) == core::kBlack);
        CHECK(fb.color(1, y) == core::kWhite);
    }
}
