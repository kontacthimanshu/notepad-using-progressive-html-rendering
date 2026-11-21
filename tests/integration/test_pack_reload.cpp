// Reloading a pack invalidates only the glyphs whose CSS changed (US6 independent test, FR-011).
#include <doctest.h>

#include <filesystem>
#include <string>

#include "pxc/fonts/font_registry.hpp"
#include "pxc/fonts/raster_cache.hpp"
#include "support/fs_util.hpp"

using namespace pxc;
namespace fs = std::filesystem;

TEST_CASE("editing one glyph and reloading invalidates only that glyph's cache entries") {
    test::TempDir tmp;
    const fs::path pack = tmp.path() / "verdana-style";
    fs::copy(fs::path(PXC_FONTS_DIR) / "verdana-style", pack, fs::copy_options::recursive);

    core::DiagnosticsLog log;
    fonts::FontRegistry reg(log);
    REQUIRE(reg.loadPack(pack) == fonts::PackState::Valid);
    fonts::RasterCache cache(reg);
    const auto oldB = cache.get("verdana-style", U'B', 1, core::kBlack, core::kWhite).pixels;
    (void)cache.get("verdana-style", U'A', 1, core::kBlack, core::kWhite);
    (void)cache.get("verdana-style", U'C', 1, core::kBlack, core::kWhite);
    const auto rev = reg.packRevision("verdana-style");

    // Change one cell of B: the top-right cell of its first row becomes inked.
    std::string b = test::readAll(pack / "glyphs/U+0042.css");
    const std::string oldRow = "\"@ @ @ @ @ .\"";
    const auto row = b.find(oldRow);
    REQUIRE(row != std::string::npos);
    b.replace(row, oldRow.size(), "\"@ @ @ @ @ @\"");
    test::writeAll(pack / "glyphs/U+0042.css", b);

    const auto res = reg.reload("verdana-style");
    CHECK(res.changedCodePoints == std::vector<char32_t>{U'B'});
    CHECK(res.newState == fonts::PackState::Valid);
    CHECK(reg.packRevision("verdana-style") == rev + 1);
    cache.evictStale("verdana-style");

    cache.resetStats();
    (void)cache.get("verdana-style", U'A', 1, core::kBlack, core::kWhite);
    (void)cache.get("verdana-style", U'C', 1, core::kBlack, core::kWhite);
    CHECK(cache.stats().hits == 2);
    CHECK(cache.stats().rasterizations == 0);
    const auto newB = cache.get("verdana-style", U'B', 1, core::kBlack, core::kWhite).pixels;
    CHECK(cache.stats().rasterizations == 1);
    CHECK(newB != oldB);

    // Corrupt C: the pack becomes Degraded and C falls back to the pack replacement with a diagnostic.
    test::writeAll(pack / "glyphs/U+0043.css", "glyph { this is not valid");
    const auto res2 = reg.reload("verdana-style");
    CHECK(res2.newState == fonts::PackState::Degraded);
    const auto c = reg.resolve("verdana-style", U'C');
    REQUIRE(c.def);
    CHECK(c.source == fonts::GlyphSource::PackReplacement);
    bool reported = false;
    for (const auto& d : log.all()) reported |= d.code == "E-GLYPH-SYNTAX" || d.code == "W-GLYPH-FALLBACK-PACK-REPLACEMENT";
    CHECK(reported);
}
