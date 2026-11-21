// Contract test for the glyph CSS parser (contracts/glyph-css-schema.md).
#include <doctest.h>

#include <algorithm>
#include <filesystem>
#include <random>
#include <sstream>

#include "pxc/glyphcss/font_manifest.hpp"
#include "pxc/glyphcss/glyph_parser.hpp"
#include "support/fs_util.hpp"

using namespace pxc;
using namespace pxc::glyphcss;
namespace fs = std::filesystem;

namespace {

std::string cpString(char32_t cp) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "U+%04X", static_cast<unsigned>(cp));
    return buf;
}

std::string describe(const GlyphDefinition& g) {
    std::ostringstream o;
    o << "codepoint " << cpString(g.codePoint) << "\n"
      << "width " << g.width << "\nheight " << g.height << "\nbaseline " << g.baseline << "\nadvance " << g.advance
      << "\nbearing " << g.bearingX << "\n";
    for (int r = 0; r < g.height; ++r) {
        o << "row";
        for (int c = 0; c < g.width; ++c) o << ' ' << cellToken(g.cellAt(c, r));
        o << "\n";
    }
    for (const auto& [k, v] : g.metadata) o << "meta " << k << ' ' << v << "\n";
    return o.str();
}

std::vector<fs::path> filesWithExt(const fs::path& dir, const std::string& ext) {
    std::vector<fs::path> out;
    for (const auto& e : fs::recursive_directory_iterator(dir))
        if (e.is_regular_file() && e.path().extension() == ext) out.push_back(e.path());
    std::sort(out.begin(), out.end());
    return out;
}

} // namespace

TEST_CASE("every valid glyph fixture parses to its expected definition") {
    const auto files = filesWithExt(fs::path(PXC_FIXTURES_DIR) / "glyphs/valid", ".css");
    REQUIRE(files.size() >= 12);
    for (const auto& f : files) {
        CAPTURE(f.filename().string());
        auto g = parseGlyph(test::readAll(f), f, "fixture-family");
        if (!g) {
            for (const auto& d : g.error()) MESSAGE(core::format(d));
        }
        REQUIRE(g);
        const fs::path exp = fs::path(f).replace_extension(".expected.txt");
        REQUIRE(fs::exists(exp));
        CHECK(describe(*g) == test::readAll(exp));
    }
}

TEST_CASE("every invalid glyph fixture reports its exact code, line and column") {
    const auto files = filesWithExt(fs::path(PXC_FIXTURES_DIR) / "glyphs/invalid", ".css");
    REQUIRE(files.size() >= 10);
    std::vector<std::string> codesSeen;
    for (const auto& f : files) {
        CAPTURE(f.generic_string());
        std::istringstream exp(test::readAll(fs::path(f).replace_extension(".expected.txt")));
        std::string code;
        int line = 0, col = 0;
        exp >> code >> line >> col;
        auto g = parseGlyph(test::readAll(f), f, "fixture-family");
        REQUIRE_FALSE(g);
        bool found = false;
        std::string all;
        for (const auto& d : g.error()) {
            all += core::format(d) + "\n";
            if (d.code == code && d.line == line && d.column == col) found = true;
        }
        INFO(all);
        CHECK(found);
        codesSeen.push_back(code);
    }
    for (const char* c : {"E-GLYPH-SCHEMA", "E-GLYPH-MISSING-PROP", "E-GLYPH-DUP-PROP", "E-GLYPH-BAD-DIM",
                          "E-GLYPH-ROW-COUNT", "E-GLYPH-ROW-LENGTH", "E-GLYPH-BAD-COLOR", "E-GLYPH-CP-MISMATCH",
                          "E-GLYPH-FAMILY-MISMATCH", "E-GLYPH-SYNTAX"}) {
        CAPTURE(c);
        CHECK(std::find(codesSeen.begin(), codesSeen.end(), c) != codesSeen.end());
    }
}

TEST_CASE("unknown properties produce W-GLYPH-UNKNOWN-PROP and still parse") {
    const std::string css = "glyph { --schema: \"pixelcss-glyph/1\"; --code-point: U+0041; --family: \"f\";"
                            " --width: 1; --height: 1; --baseline: 1; --advance: 2; --rows: \"@\";"
                            " --wobble: 3; color: red; }";
    auto g = parseGlyph(css, "inline.css");
    REQUIRE(g);
    REQUIRE(g->warnings.size() == 2);
    CHECK(g->warnings[0].code == "W-GLYPH-UNKNOWN-PROP");
    CHECK(g->warnings[1].code == "W-GLYPH-UNKNOWN-PROP");
}

TEST_CASE("content hash is identical for LF and CRLF versions of a glyph") {
    const std::string lf = test::readAll(fs::path(PXC_FIXTURES_DIR) / "glyphs/valid/U+0041.css");
    std::string crlf;
    for (char c : lf) {
        if (c == '\n') crlf += '\r';
        crlf += c;
    }
    auto a = parseGlyph(lf, "U+0041.css");
    auto b = parseGlyph(crlf, "U+0041.css");
    REQUIRE(a);
    REQUIRE(b);
    CHECK(a->contentHash == b->contentHash);
    CHECK(a->cells == b->cells);
    auto c = parseGlyph(lf + " ", "U+0041.css");
    REQUIRE(c);
    CHECK(c->contentHash != a->contentHash);
}

TEST_CASE("fuzz: random and mutated inputs never throw or crash" * doctest::description("leakcheck:off")) {
    std::mt19937 rng(20261121);
    const std::string seed = test::readAll(fs::path(PXC_FIXTURES_DIR) / "glyphs/valid/U+0041.css");
    const std::string alphabet = "glyph{}:;\"'#@.~U+-0123456789abcdefABCDEF \n\r\t/*\\,()";
    for (int iter = 0; iter < 1000; ++iter) {
        std::string s;
        if (iter % 2 == 0) {
            s = seed;
            const int edits = 1 + static_cast<int>(rng() % 8);
            for (int e = 0; e < edits && !s.empty(); ++e) {
                const std::size_t pos = rng() % s.size();
                switch (rng() % 3) {
                case 0: s.erase(pos, 1 + rng() % 4); break;
                case 1: s.insert(pos, 1, alphabet[rng() % alphabet.size()]); break;
                default: s[pos] = static_cast<char>(rng() % 256); break;
                }
            }
        } else {
            const std::size_t n = rng() % 200;
            for (std::size_t k = 0; k < n; ++k) s += static_cast<char>(rng() % 256);
        }
        CHECK_NOTHROW((void)parseGlyph(s, "fuzz.css"));
        CHECK_NOTHROW((void)parseManifest(s, "fuzz-manifest.css"));
    }
}
