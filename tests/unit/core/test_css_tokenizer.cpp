#include <doctest.h>

#include <vector>

#include "pxc/core/css_tokenizer.hpp"

using namespace pxc::core;

namespace {
std::vector<CssToken> significant(std::string_view s, std::vector<Diagnostic>* d = nullptr) {
    std::vector<Diagnostic> local;
    auto all = tokenizeCss(s, d ? *d : local);
    std::vector<CssToken> out;
    for (auto& t : all)
        if (t.kind != CssTokenKind::Whitespace) out.push_back(t);
    return out;
}
} // namespace

TEST_CASE("tokenizer: every token kind") {
    auto t = significant(R"(glyph { --w: 7; x: "a\"b" 'c'; #FF00aa 12 -3.5 50% 4px U+0041 U+0020-007E rgb( ) [ ] , @import ! })");
    std::vector<CssTokenKind> kinds;
    for (auto& k : t) kinds.push_back(k.kind);
    using K = CssTokenKind;
    const std::vector<K> expected = {K::Ident, K::LeftBrace, K::Ident, K::Colon, K::Number, K::Semicolon,
                                     K::Ident, K::Colon, K::String, K::String, K::Semicolon, K::Hash,
                                     K::Number, K::Number, K::Percentage, K::Dimension, K::UnicodeRange,
                                     K::UnicodeRange, K::Function, K::RightParen, K::LeftBracket,
                                     K::RightBracket, K::Comma, K::AtKeyword, K::Delim, K::RightBrace, K::Eof};
    REQUIRE(kinds == expected);
    CHECK(t[2].text == "--w");
    CHECK(t[4].number == 7.0);
    CHECK(t[4].isInteger);
    CHECK(t[8].text == "a\"b");
    CHECK(t[9].text == "c");
    CHECK(t[11].text == "FF00aa");
    CHECK(t[13].number == -3.5);
    CHECK_FALSE(t[13].isInteger);
    CHECK(t[15].unit == "px");
    CHECK(t[16].rangeStart == 0x41);
    CHECK(t[16].rangeEnd == 0x41);
    CHECK(t[17].rangeStart == 0x20);
    CHECK(t[17].rangeEnd == 0x7E);
    CHECK(t[18].text == "rgb");
    CHECK(t[23].text == "import");
}

TEST_CASE("tokenizer: comments between tokens are skipped") {
    auto t = significant("a/* x */b /* y\n z */ c");
    REQUIRE(t.size() == 4);
    CHECK(t[0].text == "a");
    CHECK(t[1].text == "b");
    CHECK(t[2].text == "c");
}

TEST_CASE("tokenizer: line and column tracking across LF and CRLF") {
    auto t = significant("a\r\n  b\n   c");
    REQUIRE(t.size() == 4);
    CHECK(t[0].line == 1);
    CHECK(t[0].column == 1);
    CHECK(t[1].line == 2);
    CHECK(t[1].column == 3);
    CHECK(t[2].line == 3);
    CHECK(t[2].column == 4);
}

TEST_CASE("tokenizer: unterminated string and comment produce diagnostics") {
    std::vector<Diagnostic> d;
    auto t = significant("x \"abc\ny", &d);
    REQUIRE(d.size() == 1);
    CHECK(d[0].code == "E-CSS-SYNTAX");
    CHECK(d[0].line == 1);
    CHECK(d[0].column == 3);
    CHECK(t[1].kind == CssTokenKind::BadString);

    std::vector<Diagnostic> d2;
    significant("a /* never closed", &d2);
    REQUIRE(d2.size() == 1);
    CHECK(d2[0].column == 3);
}

TEST_CASE("tokenizer: ident vs unicode-range vs negative numbers") {
    auto t = significant("U uv -x --y -1 u+?");
    CHECK(t[0].kind == CssTokenKind::Ident);
    CHECK(t[1].kind == CssTokenKind::Ident);
    CHECK(t[2].kind == CssTokenKind::Ident);
    CHECK(t[3].text == "--y");
    CHECK(t[4].kind == CssTokenKind::Number);
    CHECK(t[4].number == -1.0);
}
