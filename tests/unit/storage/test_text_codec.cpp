// Text codec: encodings, BOMs, line endings, byte-exact round trips (research R-11, FR-032).
#include <doctest.h>

#include <filesystem>

#include "pxc/storage/text_codec.hpp"
#include "support/fs_util.hpp"

using namespace pxc;
using namespace pxc::storage;
namespace fs = std::filesystem;

namespace {
std::string fixture(const char* name) { return test::readAll(fs::path(PXC_FIXTURES_DIR) / "text" / name); }
LoadedText decodeOk(const char* name) {
    auto r = decodeText(fixture(name), InvalidUtf8Policy::Fail);
    REQUIRE(r);
    return *r;
}
} // namespace

TEST_CASE("LF, CRLF and mixed line endings are detected and stripped") {
    auto lf = decodeOk("lf.txt");
    CHECK(lf.info.eol == Eol::LF);
    CHECK(lf.text == U"first line\nsecond line\nthird\n");
    auto crlf = decodeOk("crlf.txt");
    CHECK(crlf.info.eol == Eol::CRLF);
    CHECK(crlf.text == lf.text);
    auto mixed = decodeOk("mixed.txt");
    CHECK(mixed.info.eol == Eol::Mixed);
    CHECK(mixed.text == U"one\ntwo\nthree\n");
    for (const auto& t : {lf, crlf, mixed}) CHECK(t.text.find(U'\r') == std::u32string::npos);
}

TEST_CASE("BOMs are detected; UTF-16 is decoded") {
    auto bom = decodeOk("utf8-bom.txt");
    CHECK(bom.info.encoding == Encoding::Utf8Bom);
    CHECK(bom.text == U"café au lait\nline 2");
    auto le = decodeOk("utf16le-bom.txt");
    CHECK(le.info.encoding == Encoding::Utf16LE);
    CHECK(le.text == U"UTF-16 LE text\nété");
    auto be = decodeOk("utf16be-bom.txt");
    CHECK(be.info.encoding == Encoding::Utf16BE);
    CHECK(be.text == U"UTF-16 BE text\nnaïve");
}

TEST_CASE("invalid UTF-8: Fail reports InvalidEncoding, Replace marks the document converted") {
    auto f = decodeText(fixture("invalid-utf8.txt"), InvalidUtf8Policy::Fail);
    REQUIRE_FALSE(f);
    CHECK(f.error().kind == FileError::Kind::InvalidEncoding);
    CHECK_FALSE(f.error().userMessage.empty());
    auto r = decodeText(fixture("invalid-utf8.txt"), InvalidUtf8Policy::Replace);
    REQUIRE(r);
    CHECK(r->info.converted);
    CHECK(r->text.find(U'�') != std::u32string::npos);
    CHECK(r->text.substr(0, 5) == U"good ");
}

TEST_CASE("decode then encode reproduces the original bytes exactly") {
    for (const char* name : {"lf.txt", "crlf.txt", "utf8-bom.txt", "trailing-newline.txt", "no-trailing-newline.txt",
                             "empty.txt", "tabs.txt", "unicode.txt"}) {
        CAPTURE(name);
        const std::string bytes = fixture(name);
        auto d = decodeText(bytes, InvalidUtf8Policy::Fail);
        REQUIRE(d);
        CHECK(encodeText(d->text, d->info) == bytes);
    }
}

TEST_CASE("mixed and new documents are written with CRLF; UTF-16 is written as UTF-8") {
    auto mixed = decodeOk("mixed.txt");
    CHECK(encodeText(mixed.text, mixed.info) == "one\r\ntwo\r\nthree\r\n");
    FileInfo fresh;
    CHECK(encodeText(U"a\nb", fresh) == "a\r\nb");
    auto le = decodeOk("utf16le-bom.txt");
    CHECK(encodeText(le.text, le.info) == "UTF-16 LE text\r\n\xc3\xa9t\xc3\xa9");
}
