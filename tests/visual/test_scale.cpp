// Integer scale policy and scale-2 rendering (research R-08, FR-015, spec edge case on display scaling).
#include <doctest.h>

#include "pxc/core/scale.hpp"
#include "pxc/fonts/text_line.hpp"
#include "support/editor_harness.hpp"
#include "support/golden.hpp"

using namespace pxc;

TEST_CASE("DPI to integer scale mapping") {
    CHECK(core::scaleForDpi(96) == 1);  // 100 %
    CHECK(core::scaleForDpi(120) == 1); // 125 %
    CHECK(core::scaleForDpi(144) == 2); // 150 %
    CHECK(core::scaleForDpi(168) == 2); // 175 %
    CHECK(core::scaleForDpi(192) == 2); // 200 %
    CHECK(core::scaleForDpi(240) == 3); // 250 %
    CHECK(core::scaleForDpi(0) == 1);
}

TEST_CASE("scale-2 glyph golden is an exact 2x2 cell replication of scale 1") {
    core::DiagnosticsLog log;
    fonts::FontRegistry reg(log);
    reg.loadAll(PXC_FONTS_DIR);
    fonts::RasterCache cache(reg);
    const auto s1 = fonts::renderTextLine(reg, cache, "verdana-style", U"A", 1, core::kBlack, core::kWhite);
    const auto s2 = fonts::renderTextLine(reg, cache, "verdana-style", U"A", 2, core::kBlack, core::kWhite);
    REQUIRE(s2.width() == 2 * s1.width());
    REQUIRE(s2.height() == 2 * s1.height());
    for (int y = 0; y < s2.height(); ++y)
        for (int x = 0; x < s2.width(); ++x) CHECK(s2.pixel(x, y) == s1.pixel(x / 2, y / 2));
    CHECK_GOLDEN(s2, "glyphs/verdana-style/U+0041.s2.bmp");
}

TEST_CASE("editor surface at scale 2") {
    test::EditorHarness h(640, 240, false, 2);
    h.type(U"The quick brown fox\njumps over the lazy dog.\n0123456789 !?");
    h.paint();
    CHECK(h.layout->rowHeight() == 2 * 15);
    CHECK_GOLDEN(h.fb, "editor/basic.s2.bmp");
}
