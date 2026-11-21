#include <doctest.h>

#include "pxc/render/bmp_io.hpp"
#include "pxc/render/framebuffer.hpp"

using namespace pxc;
using namespace pxc::render;

TEST_CASE("framebuffer fill and clip") {
    Framebuffer fb(8, 4, core::kWhite);
    fb.fillRect({-2, -2, 4, 4}, core::kBlack);
    CHECK(fb.color(0, 0) == core::kBlack);
    CHECK(fb.color(1, 1) == core::kBlack);
    CHECK(fb.color(2, 0) == core::kWhite);
    fb.fillRect({6, 2, 10, 10}, core::Color{1, 2, 3, 255});
    CHECK(fb.color(7, 3) == core::Color{1, 2, 3, 255});
    CHECK(fb.color(5, 3) == core::kWhite);
    fb.fillRect({100, 100, 4, 4}, core::kBlack); // fully outside: no-op
}

TEST_CASE("framebuffer rejects oversized dimensions") {
    auto r = Framebuffer::create(kMaxFramebufferDim + 1, 1);
    CHECK_FALSE(r);
    auto ok = Framebuffer::create(3, 2);
    REQUIRE(ok);
    CHECK(ok->width() == 3);
}

TEST_CASE("bmp write -> read round-trip is identical") {
    Framebuffer fb(5, 3, core::kTransparent);
    for (int y = 0; y < 3; ++y)
        for (int x = 0; x < 5; ++x)
            fb.setPixel(x, y, core::Color{static_cast<std::uint8_t>(x * 40), static_cast<std::uint8_t>(y * 80), 7,
                                          static_cast<std::uint8_t>(255 - x)}.toBGRA());
    auto back = decodeBmp32(encodeBmp32(fb));
    REQUIRE(back);
    CHECK(*back == fb);
}

TEST_CASE("reading a truncated BMP returns an error") {
    Framebuffer fb(4, 4, core::kWhite);
    std::string bytes = encodeBmp32(fb);
    bytes.resize(bytes.size() - 5);
    CHECK_FALSE(decodeBmp32(bytes));
    CHECK_FALSE(decodeBmp32("XX"));
}
