// Contract test for FontRegistry: bundled packs and the deterministic fallback chain (research R-07).
#include <doctest.h>

#include <algorithm>
#include <filesystem>

#include "pxc/fonts/font_registry.hpp"

using namespace pxc;
using namespace pxc::fonts;
namespace fs = std::filesystem;

namespace {
struct Reg {
    core::DiagnosticsLog log;
    FontRegistry reg{log};
    Reg() {
        reg.loadAll(PXC_FONTS_DIR);
        reg.loadPack(fs::path(PXC_FIXTURES_DIR) / "packs/missing-glyph");
        reg.loadPack(fs::path(PXC_FIXTURES_DIR) / "packs/no-replacement");
    }
    std::size_t count(const std::string& code) const {
        return static_cast<std::size_t>(std::count_if(log.all().begin(), log.all().end(),
                                                      [&](const core::Diagnostic& d) { return d.code == code; }));
    }
};
} // namespace

TEST_CASE("three bundled packs load as Valid in manifest order; Verdana is the default") {
    Reg r;
    std::vector<std::string> bundled;
    for (const auto& p : r.reg.packs())
        if (p.id == "verdana-style" || p.id == "arial-narrow-style" || p.id == "comic-style") {
            CHECK(p.state == PackState::Valid);
            bundled.push_back(p.id);
        }
    CHECK(bundled == std::vector<std::string>{"verdana-style", "arial-narrow-style", "comic-style"});
    CHECK(std::string(kDefaultPackId) == "verdana-style");
    CHECK(r.reg.packs().front().id == "verdana-style");
}

TEST_CASE("missing glyph resolves to the pack's own U+FFFD") {
    Reg r;
    const auto g = r.reg.resolve("missing-glyph", U'B');
    REQUIRE(g.def);
    CHECK(g.source == GlyphSource::PackReplacement);
    CHECK(g.def->codePoint == 0xFFFD);
    CHECK(g.packUsed == "missing-glyph");
    CHECK(r.reg.resolve("missing-glyph", U'A').source == GlyphSource::Primary);
}

TEST_CASE("without a pack replacement the engine replacement glyph is used") {
    Reg r;
    const auto g = r.reg.resolve("no-replacement", U'Z');
    REQUIRE(g.def);
    CHECK(g.source == GlyphSource::EngineReplacement);
    CHECK(g.def->width >= 3);
    CHECK(g.def->baseline == g.def->height);
    CHECK(g.def->advance == g.def->width + 1);
    CHECK(g.def->hasInk());
}

TEST_CASE("unknown pack ids resolve through verdana-style, then the engine box font") {
    Reg r;
    const auto g = r.reg.resolve("no-such-pack", U'A');
    REQUIRE(g.def);
    CHECK(g.packUsed == "verdana-style");
    CHECK(g.source == GlyphSource::Primary);

    core::DiagnosticsLog empty;
    FontRegistry none(empty);
    const auto box = none.resolve("anything", U'A');
    REQUIRE(box.def);
    CHECK(box.source == GlyphSource::EngineReplacement);
}

TEST_CASE("non-ASCII code points resolve to the replacement glyph") {
    Reg r;
    const auto g = r.reg.resolve("verdana-style", U'é');
    REQUIRE(g.def);
    CHECK(g.source == GlyphSource::PackReplacement);
    CHECK(g.def->codePoint == 0xFFFD);
}

TEST_CASE("each (pack, code point, reason) fallback is logged exactly once") {
    Reg r;
    for (int i = 0; i < 100; ++i) {
        (void)r.reg.resolve("missing-glyph", U'B');
        (void)r.reg.resolve("no-replacement", U'Z');
        (void)r.reg.resolve("verdana-style", U'é');
    }
    CHECK(r.count("W-GLYPH-FALLBACK-PACK-REPLACEMENT") == 2);
    CHECK(r.count("W-GLYPH-FALLBACK-ENGINE") == 1);
}
