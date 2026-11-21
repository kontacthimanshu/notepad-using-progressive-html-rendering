// Visual test: deterministic fallback glyphs (FR-010, US3-AS5).
#include <doctest.h>

#include <filesystem>

#include "pxc/fonts/font_registry.hpp"
#include "pxc/fonts/raster_cache.hpp"
#include "pxc/fonts/text_line.hpp"
#include "support/golden.hpp"

using namespace pxc;
namespace fs = std::filesystem;

namespace {
struct Env {
    core::DiagnosticsLog log;
    fonts::FontRegistry reg{log};
    Env() {
        reg.loadAll(PXC_FONTS_DIR);
        reg.loadPack(fs::path(PXC_FIXTURES_DIR) / "packs/missing-glyph");
        reg.loadPack(fs::path(PXC_FIXTURES_DIR) / "packs/no-replacement");
    }
    render::Framebuffer line(std::string_view pack, std::u32string_view text) {
        fonts::RasterCache cache(reg);
        return fonts::renderTextLine(reg, cache, pack, text, 2, core::kBlack, core::kWhite);
    }
};
} // namespace

TEST_CASE("missing glyph renders the pack replacement") {
    Env e;
    CHECK_GOLDEN(e.line("missing-glyph", U"ABC"), "fallback/missing-glyph.bmp");
}

TEST_CASE("non-ASCII characters render the replacement glyph in every bundled pack") {
    Env e;
    for (const char* pack : {"verdana-style", "arial-narrow-style", "comic-style"}) {
        CAPTURE(pack);
        CHECK_GOLDEN(e.line(pack, U"aé漢b"), std::string("fallback/non-ascii-") + pack + ".bmp");
    }
}

TEST_CASE("engine box glyph when the pack has no replacement") {
    Env e;
    CHECK_GOLDEN(e.line("no-replacement", U"AZB"), "fallback/engine-box.bmp");
}
