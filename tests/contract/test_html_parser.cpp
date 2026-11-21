// Contract test for the restricted HTML parser (contracts/html-subset.md, FR-035/FR-036/FR-040).
#include <doctest.h>

#include <filesystem>
#include <random>
#include <sstream>

#include "pxc/core/unicode.hpp"
#include "pxc/html/document_node.hpp"
#include "support/fs_util.hpp"

using namespace pxc;
namespace fs = std::filesystem;

namespace {
fs::path dir() { return fs::path(PXC_FIXTURES_DIR) / "html"; }
html::PreviewDocument parseFile(const std::string& name) {
    const auto text = *core::decodeUtf8(test::readAll(dir() / (name + ".html")), core::InvalidPolicy::Replace);
    auto d = html::parseHtml(text);
    REQUIRE(d);
    return std::move(*d);
}
std::string codes(const html::PreviewDocument& d) {
    std::string s;
    for (const auto& x : d.diagnostics) s += x.code + "\n";
    return s;
}
bool containsText(const html::DocumentNode& n, std::u32string_view needle) {
    if (n.tag == html::Tag::Text && n.text.find(needle) != std::u32string::npos) return true;
    for (const auto& c : n.children)
        if (containsText(*c, needle)) return true;
    return false;
}
} // namespace

TEST_CASE("every fixture parses to its expected tree and diagnostics") {
    for (const char* name : {"basic", "nested", "styles", "unsupported", "malformed-unclosed-attr", "malformed-misnest",
                             "malformed-stray-end"}) {
        const std::string fixture = name;
        CAPTURE(fixture);
        const auto d = parseFile(name);
        const std::string actual = html::dumpTree(d);
        INFO("actual tree:\n" << actual);
        CHECK(actual == test::readAll(dir() / (fixture + ".expected-tree.txt")));
        CHECK(codes(d) == test::readAll(dir() / (std::string(name) + ".expected-diagnostics.txt")));
    }
}

TEST_CASE("entities are decoded") {
    auto d = html::parseHtml(U"<p>&amp;&lt;&gt;&quot;&apos;&#39;&#65;&#x42;&nbsp;&unknown;</p>");
    REQUIRE(d);
    const auto& p = *d->root->children[1]->children[0];
    REQUIRE(p.children.size() == 1);
    CHECK(p.children[0]->text == U"&<>\"''AB &unknown;");
    bool entityWarning = false;
    for (const auto& x : d->diagnostics) entityWarning |= x.code == "W-HTML-ENTITY";
    CHECK(entityWarning);
}

TEST_CASE("script and style contents never become text; event handlers are removed") {
    const auto d = parseFile("unsupported");
    CHECK_FALSE(containsText(*d.root, U"alert"));
    CHECK_FALSE(containsText(*d.root, U"injected"));
    CHECK_FALSE(containsText(*d.root, U"@import"));
    CHECK(d.styleSheets.size() == 1);
    CHECK(html::dumpTree(d).find("onclick") == std::string::npos);
}

TEST_CASE("title text is extracted") {
    const auto d = parseFile("basic");
    CHECK(d.title == U"Basic & simple");
    CHECK_FALSE(d.hasErrors());
    CHECK(parseFile("malformed-unclosed-attr").hasErrors());
}

TEST_CASE("fuzz: random and mutated HTML never crashes" * doctest::description("leakcheck:off")) {
    std::mt19937 rng(1234);
    const std::u32string seed = *core::decodeUtf8(test::readAll(dir() / "styles.html"), core::InvalidPolicy::Replace);
    const std::u32string alphabet = U"<>/=\"' &;#xabcdivpspanbr!-\n\tscriptstyle";
    for (int i = 0; i < 1000; ++i) {
        std::u32string s = seed;
        for (int k = 0; k < 1 + static_cast<int>(rng() % 10) && !s.empty(); ++k) {
            const std::size_t at = rng() % s.size();
            switch (rng() % 3) {
            case 0: s.erase(at, 1 + rng() % 6); break;
            case 1: s.insert(at, 1, alphabet[rng() % alphabet.size()]); break;
            default: s[at] = static_cast<char32_t>(rng() % 0x80); break;
            }
        }
        CHECK_NOTHROW((void)html::parseHtml(s));
    }
}
