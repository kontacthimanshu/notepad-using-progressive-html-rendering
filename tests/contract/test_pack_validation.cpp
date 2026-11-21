// Contract test for font-pack validation (contracts/font-manifest-schema.md, US6, SC-002).
#include <doctest.h>

#include <algorithm>
#include <filesystem>

#include "pxc/fonts/font_registry.hpp"

using namespace pxc;
using fonts::PackState;
namespace fs = std::filesystem;

namespace {
fs::path packs() { return fs::path(PXC_FIXTURES_DIR) / "packs"; }
bool hasCode(const std::vector<core::Diagnostic>& ds, const std::string& code) {
    return std::any_of(ds.begin(), ds.end(), [&](const core::Diagnostic& d) { return d.code == code; });
}
} // namespace

TEST_CASE("each fixture pack yields its expected code and state") {
    struct Case {
        const char* dir;
        const char* code;
        PackState state;
    };
    const Case cases[] = {
        {"bad-metrics", "E-PACK-METRICS", PackState::Invalid},
        {"missing-license", "E-PACK-LICENSE-FILE", PackState::Invalid},
        {"bad-authenticity", "E-PACK-AUTHENTICITY", PackState::Invalid},
        {"missing-glyph", "E-PACK-COVERAGE-MISSING", PackState::Degraded},
        {"dup-glyph", "E-PACK-DUP-GLYPH", PackState::Degraded},
        {"extra-glyph", "W-PACK-EXTRA-GLYPH", PackState::Valid},
        {"exceeds-metrics", "W-GLYPH-EXCEEDS-METRICS", PackState::Valid},
        {"untrimmed", "W-GLYPH-UNTRIMMED", PackState::Valid},
    };
    for (const auto& c : cases) {
        const std::string dir = c.dir;
        CAPTURE(dir);
        core::DiagnosticsLog log;
        fonts::FontRegistry reg(log);
        CHECK(reg.loadPack(packs() / c.dir) == c.state);
        const auto info = reg.packs();
        REQUIRE(info.size() == 1);
        CHECK(hasCode(reg.packDiagnostics(info[0].id), c.code));
    }
}

TEST_CASE("a second pack with an already registered id is rejected") {
    core::DiagnosticsLog log;
    fonts::FontRegistry reg(log);
    reg.loadAll(PXC_FONTS_DIR);
    CHECK(reg.loadPack(packs() / "dup-id/verdana-style") == PackState::Invalid);
    CHECK(std::any_of(log.all().begin(), log.all().end(), [](const core::Diagnostic& d) { return d.code == "E-PACK-DUP-ID"; }));
    CHECK(reg.pack("verdana-style")->state == PackState::Valid); // the original stays active
}

TEST_CASE("bundled packs: Valid, 95 ASCII + U+FFFD each, 285 ASCII files in total, full provenance") {
    core::DiagnosticsLog log;
    fonts::FontRegistry reg(log);
    reg.loadAll(PXC_FONTS_DIR);
    std::size_t totalAscii = 0;
    for (const char* id : {"verdana-style", "arial-narrow-style", "comic-style"}) {
        CAPTURE(std::string(id));
        const auto p = reg.pack(id);
        REQUIRE(p);
        CHECK(p->state == PackState::Valid);
        std::size_t ascii = 0;
        bool fffd = false;
        for (const auto* g : reg.glyphs(id)) {
            if (g->codePoint >= 0x20 && g->codePoint <= 0x7E) ++ascii;
            fffd |= g->codePoint == 0xFFFD;
        }
        CHECK(ascii == 95);
        CHECK(fffd);
        totalAscii += ascii;
        CHECK_FALSE(p->origin.empty());
        CHECK_FALSE(p->author.empty());
        CHECK_FALSE(p->version.empty());
        CHECK_FALSE(p->license.empty());
        CHECK(p->authenticity == "style-interpretation");
        CHECK(reg.packDiagnostics(id).empty());
    }
    CHECK(totalAscii == 285);
}

TEST_CASE("an additional conforming pack registers next to the bundled packs without code changes") {
    core::DiagnosticsLog log;
    fonts::FontRegistry reg(log);
    reg.loadAll(PXC_FONTS_DIR);
    CHECK(reg.loadPack(packs() / "extra-pack") == PackState::Valid);
    const auto all = reg.packs();
    CHECK(all.size() == 4);
    CHECK(all[3].id == "extra-pack"); // --order 50 sorts after the bundled packs
    CHECK(reg.packForAlias("Extra Test Pack") == "extra-pack");
    const auto g = reg.resolve("extra-pack", U'Q');
    REQUIRE(g.def);
    CHECK(g.source == fonts::GlyphSource::Primary);
}
