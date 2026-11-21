#include <doctest.h>

#include "pxc/core/unicode.hpp"

using namespace pxc::core;

TEST_CASE("utf8 decodes valid ASCII, 2-, 3- and 4-byte sequences") {
    auto r = decodeUtf8("A\xC3\xA9\xE6\xBC\xA2\xF0\x9F\x98\x80", InvalidPolicy::Fail);
    REQUIRE(r);
    CHECK(*r == U"Aé漢\U0001F600");
}

TEST_CASE("utf8 rejects overlong encodings") {
    auto r = decodeUtf8("\xC0\xAF", InvalidPolicy::Fail);
    REQUIRE_FALSE(r);
    CHECK(r.error().byteOffset == 0);
    auto rr = decodeUtf8("a\xE0\x80\xAF", InvalidPolicy::Replace);
    REQUIRE(rr);
    CHECK(*rr == U"a�");
}

TEST_CASE("utf8 rejects surrogates and values above U+10FFFF") {
    CHECK_FALSE(decodeUtf8("\xED\xA0\x80", InvalidPolicy::Fail));
    CHECK_FALSE(decodeUtf8("\xF4\x90\x80\x80", InvalidPolicy::Fail));
}

TEST_CASE("utf8 truncated sequence reports offset and replaces once") {
    auto f = decodeUtf8("ab\xE6\xBC", InvalidPolicy::Fail);
    REQUIRE_FALSE(f);
    CHECK(f.error().byteOffset == 2);
    auto r = decodeUtf8("ab\xE6\xBC", InvalidPolicy::Replace);
    REQUIRE(r);
    CHECK(*r == U"ab�");
}

TEST_CASE("utf8 encode round-trips") {
    const std::u32string s = U"xé漢\U0001F600";
    auto back = decodeUtf8(encodeUtf8(s), InvalidPolicy::Fail);
    REQUIRE(back);
    CHECK(*back == s);
}

TEST_CASE("utf16 surrogate pairs and lone surrogates") {
    const std::u16string pair = u"\U0001F600";
    CHECK(decodeUtf16(pair) == U"\U0001F600");
    const char16_t lone[] = {0xD800, u'a', 0xDC00};
    CHECK(decodeUtf16(lone) == U"�a�");
    CHECK(encodeUtf16(U"\U0001F600") == pair);
}

TEST_CASE("wide conversions") {
    CHECK(toWide(U"hé") == L"hé");
    CHECK(fromWide(L"\U0001F600") == U"\U0001F600");
    CHECK(wideToUtf8(utf8ToWide("caf\xC3\xA9")) == "caf\xC3\xA9");
}
