// Visual test: glyph rendering through pxc_fonts + pxc_render (US1, FR-014/FR-015/FR-052).
#include <doctest.h>

#include <filesystem>

#include "pxc/fonts/font_registry.hpp"
#include "pxc/fonts/raster_cache.hpp"
#include "pxc/fonts/rasterize.hpp"
#include "pxc/fonts/text_line.hpp"
#include "pxc/glyphcss/glyph_parser.hpp"
#include "pxc/render/renderer.hpp"
#include "support/fs_util.hpp"
#include "support/golden.hpp"

using namespace pxc;
namespace fs = std::filesystem;

namespace {

glyphcss::GlyphDefinition fixture(const char* name) {
    const fs::path p = fs::path(PXC_FIXTURES_DIR) / "glyphs/valid" / name;
    auto g = glyphcss::parseGlyph(test::readAll(p), p);
    REQUIRE(g);
    return *g;
}

// Canvas for a single fixture glyph: (advance+2)·s by (height+2)·s, pen at s, baseline at (baseline+1)·s.
render::Framebuffer canvasFor(const glyphcss::GlyphDefinition& g, int s, core::Color bg, int glyphs = 1) {
    return render::Framebuffer((g.advance * glyphs + 2) * s, (g.height + 2) * s, bg);
}

} // namespace

TEST_CASE("U+0041 fixture renders pixel-exact at scale 1 and 2") {
    const auto g = fixture("U+0041.css");
    for (int s : {1, 2}) {
        CAPTURE(s);
        auto fb = canvasFor(g, s, core::kWhite);
        render::Renderer r;
        r.blitGlyph(fb, fonts::rasterize(g, s, core::kBlack, core::kWhite), s, (g.baseline + 1) * s, fb.bounds());
        // Every opaque cell maps to an s×s block at its declared position; transparent cells keep the background.
        for (int row = 0; row < g.height; ++row)
            for (int col = 0; col < g.width; ++col)
                for (int dy = 0; dy < s; ++dy)
                    for (int dx = 0; dx < s; ++dx) {
                        const bool ink = g.cellAt(col, row).kind == glyphcss::CellKind::CurrentColor;
                        const auto px = fb.color(s + col * s + dx, s + row * s + dy);
                        CHECK(px == (ink ? core::kBlack : core::kWhite));
                    }
        CHECK_GOLDEN(fb, s == 1 ? "glyphs/fixture/U+0041.s1.bmp" : "glyphs/fixture/U+0041.s2.bmp");
    }
}

TEST_CASE("two consecutive glyphs follow advance and share the baseline") {
    const auto g = fixture("U+0041.css");
    const int s = 2;
    auto fb = canvasFor(g, s, core::kWhite, 2);
    render::Renderer r;
    const auto raster = fonts::rasterize(g, s, core::kBlack, core::kWhite);
    const int baseline = (g.baseline + 1) * s;
    r.blitGlyph(fb, raster, s, baseline, fb.bounds());
    r.blitGlyph(fb, raster, s + g.advance * s, baseline, fb.bounds());
    for (int y = 0; y < fb.height(); ++y)
        for (int x = 0; x < g.advance * s; ++x) {
            CAPTURE(x);
            CAPTURE(y);
            CHECK(fb.pixel(s + x, y) == fb.pixel(s + g.advance * s + x, y));
        }
    CHECK_GOLDEN(fb, "glyphs/fixture/AA.s2.bmp");
}

TEST_CASE("transparency, literal colours and background substitution") {
    const core::Color bg{0x30, 0x60, 0x90, 0xFF};
    for (const char* name : {"multicolor.css", "alpha-zero.css"}) {
        CAPTURE(name);
        const auto g = fixture(name);
        auto fb = canvasFor(g, 3, bg);
        render::Renderer r;
        r.blitGlyph(fb, fonts::rasterize(g, 3, core::kWhite, bg), 3, (g.baseline + 1) * 3, fb.bounds());
        CHECK_GOLDEN(fb, std::string("glyphs/fixture/") + fs::path(name).stem().string() + ".s3.bmp");
    }
    // alpha-zero: first cell #FF000000 is transparent -> background untouched
    {
        const auto g = fixture("alpha-zero.css");
        auto fb = canvasFor(g, 1, bg);
        render::Renderer r;
        r.blitGlyph(fb, fonts::rasterize(g, 1, core::kWhite, bg), 1, g.baseline + 1, fb.bounds());
        CHECK(fb.color(1, 1) == bg);
        CHECK(fb.color(2, 1) == core::Color{255, 0, 0, 255});
    }
    // bgcolor: knock-out cells must equal the run background exactly.
    {
        const core::Color paper{0xFF, 0xEE, 0x88, 0xFF};
        const auto g = fixture("bgcolor.css");
        auto fb = canvasFor(g, 1, paper);
        render::Renderer r;
        r.blitGlyph(fb, fonts::rasterize(g, 1, core::kBlack, paper), 1, g.baseline + 1, fb.bounds());
        CHECK(fb.color(1 + 1, 1 + 1) == paper);
        CHECK(fb.color(1 + 2, 1 + 2) == paper);
        CHECK(fb.color(1 + 0, 1 + 0) == core::kBlack);
        CHECK_GOLDEN(fb, "glyphs/fixture/bgcolor.s1.bmp");
    }
}

TEST_CASE("glyph clipped by a rectangle") {
    const auto g = fixture("U+0041.css");
    auto fb = canvasFor(g, 2, core::kWhite);
    render::Renderer r;
    const core::Rect clip{0, 0, fb.width(), fb.height() / 2};
    r.blitGlyph(fb, fonts::rasterize(g, 2, core::kBlack, core::kWhite), 2, (g.baseline + 1) * 2, clip);
    for (int y = clip.bottom(); y < fb.height(); ++y)
        for (int x = 0; x < fb.width(); ++x) CHECK(fb.color(x, y) == core::kWhite);
    CHECK_GOLDEN(fb, "glyphs/fixture/clipped.s2.bmp");
}

TEST_CASE("baseline alignment of a descender glyph next to a cap glyph") {
    const auto cap = fixture("U+0041.css");
    const auto desc = fixture("nonsquare.css"); // baseline 7 of 9: two descender rows
    const int s = 2;
    render::Framebuffer fb((cap.advance + desc.advance + 2) * s, (cap.height + 2 + 2) * s, core::kWhite);
    render::Renderer r;
    const int baseline = (cap.baseline + 1) * s;
    r.blitGlyph(fb, fonts::rasterize(cap, s, core::kBlack, core::kWhite), s, baseline, fb.bounds());
    r.blitGlyph(fb, fonts::rasterize(desc, s, core::kBlack, core::kWhite), s + cap.advance * s, baseline, fb.bounds());
    // The descender's bottom row is (height - baseline) rows below the shared baseline.
    const int descX = s + cap.advance * s + desc.bearingX * s;
    CHECK(fb.color(descX, baseline + (desc.height - desc.baseline - 1) * s) == core::kBlack);
    CHECK(fb.color(descX, baseline - desc.baseline * s) == core::kBlack);
    CHECK(fb.color(descX, baseline - desc.baseline * s - 1) == core::kWhite);
    CHECK_GOLDEN(fb, "glyphs/fixture/baseline.s2.bmp");
}

TEST_CASE("renderTextLine produces the canonical canvas (research R-21)") {
    core::DiagnosticsLog log;
    fonts::FontRegistry reg(log);
    reg.loadAll(PXC_FONTS_DIR);
    REQUIRE(reg.isUsable("verdana-style"));
    fonts::RasterCache cache(reg);
    const std::u32string text = U"Ag";
    for (int s : {1, 2}) {
        CAPTURE(s);
        const auto fm = reg.metrics("verdana-style");
        int sum = 0;
        for (char32_t c : text) sum += reg.resolve("verdana-style", c).def->advance;
        const auto fb = fonts::renderTextLine(reg, cache, "verdana-style", text, s, core::kBlack, core::kWhite);
        CHECK(fb.width() == (sum + 2) * s);
        CHECK(fb.height() == fm.lineHeight * s);
        // The first column (pen starts at x = s) is never inked for a glyph with bearing 0.
        for (int y = 0; y < fb.height(); ++y) CHECK(fb.color(0, y) == core::kWhite);
        // The 'A' cap reaches exactly ascent - capHeight rows below the top edge.
        const auto& a = *reg.resolve("verdana-style", U'A').def;
        const int top = (fm.ascent - a.baseline) * s;
        bool inkOnTopRow = false;
        for (int x = 0; x < fb.width(); ++x) inkOnTopRow |= fb.color(x, top) == core::kBlack;
        CHECK(inkOnTopRow);
    }
}
