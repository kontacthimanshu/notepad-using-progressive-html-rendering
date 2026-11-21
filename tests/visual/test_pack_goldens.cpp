// Every bundled glyph is compared pixel-exactly with its approved golden (SC-003).
#include <doctest.h>

#include <cstdio>

#include "pxc/fonts/font_registry.hpp"
#include "pxc/fonts/raster_cache.hpp"
#include "pxc/fonts/text_line.hpp"
#include "support/golden.hpp"

using namespace pxc;

TEST_CASE("every glyph of every bundled pack matches its golden at s=1") {
    core::DiagnosticsLog log;
    fonts::FontRegistry reg(log);
    reg.loadAll(PXC_FONTS_DIR);
    const auto packs = reg.packs();
    REQUIRE_FALSE(packs.empty());
    std::size_t checked = 0;
    for (const auto& p : packs) {
        CAPTURE(p.id);
        CHECK(p.state == fonts::PackState::Valid);
        fonts::RasterCache cache(reg);
        for (const auto* g : reg.glyphs(p.id)) {
            char name[64];
            std::snprintf(name, sizeof name, "glyphs/%s/U+%04X.s1.bmp", p.id.c_str(), static_cast<unsigned>(g->codePoint));
            const std::u32string text(1, g->codePoint);
            const auto fb = fonts::renderTextLine(reg, cache, p.id, text, 1, core::kBlack, core::kWhite);
            CHECK_GOLDEN(fb, name);
            ++checked;
        }
    }
    MESSAGE("checked " << checked << " glyph goldens");
    CHECK(checked >= 96);
}
