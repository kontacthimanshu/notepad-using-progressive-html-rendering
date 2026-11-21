// Contract test for the presentation-CSS subset and the cascade (contracts/html-subset.md, FR-039).
#include <doctest.h>

#include <algorithm>
#include <filesystem>

#include "pxc/core/unicode.hpp"
#include "pxc/html/document_node.hpp"
#include "pxc/presentcss/stylesheet.hpp"
#include "support/fs_util.hpp"

using namespace pxc;
using presentcss::Property;
namespace fs = std::filesystem;

namespace {
std::size_t count(const core::DiagnosticsLog& log, const std::string& code) {
    return static_cast<std::size_t>(std::count_if(log.all().begin(), log.all().end(),
                                                  [&](const core::Diagnostic& d) { return d.code == code; }));
}
struct Styled {
    core::DiagnosticsLog log;
    fonts::FontRegistry reg{log};
    html::PreviewDocument doc;
    explicit Styled(std::u32string_view src) {
        reg.loadAll(PXC_FONTS_DIR);
        doc = std::move(*html::parseHtml(src));
        html::computeStyles(doc, reg, "verdana-style", log);
    }
    const html::DocumentNode& body() const { return *doc.root->children[1]; }
};
} // namespace

TEST_CASE("colour values") {
    CHECK(presentcss::parseColor("#abc") == core::Color{0xAA, 0xBB, 0xCC, 255});
    CHECK(presentcss::parseColor("#336699") == core::Color{0x33, 0x66, 0x99, 255});
    CHECK(presentcss::parseColor("#33669980") == core::Color{0x33, 0x66, 0x99, 0x80});
    CHECK(presentcss::parseColor("rgb(128, 0, 255)") == core::Color{128, 0, 255, 255});
    CHECK(presentcss::parseColor("yellow") == core::Color{255, 255, 0, 255});
    CHECK(presentcss::parseColor("Navy") == core::Color{0, 0, 128, 255});
    CHECK(presentcss::parseColor("transparent")->a == 0);
    CHECK_FALSE(presentcss::parseColor("chartreuse"));
    CHECK_FALSE(presentcss::parseColor("#12"));
}

TEST_CASE("every supported property and value form parses") {
    core::DiagnosticsLog log;
    const auto d = presentcss::parseInlineStyle(
        "color: red; background-color: #0f0; font-family: \"Comic Sans MS\", serif; margin-top: 4px; "
        "margin-bottom: 1.5em; line-height: 1.25; display: none",
        log);
    REQUIRE(d.size() == 7);
    CHECK(d[0].property == Property::Color);
    CHECK(d[1].color == core::Color{0, 255, 0, 255});
    CHECK(d[2].families == std::vector<std::string>{"Comic Sans MS", "serif"});
    CHECK(d[3].length->value == 4);
    CHECK(d[4].length->unit == presentcss::Unit::Em);
    CHECK(d[5].length->unit == presentcss::Unit::None);
    CHECK(d[6].display == presentcss::DisplayValue::None);
    CHECK(log.size() == 0);
    auto lh = presentcss::parseInlineStyle("line-height: normal; line-height: 18px; margin-top: 0", log);
    REQUIRE(lh.size() == 3);
    CHECK(lh[0].lineHeightNormal);
    CHECK(lh[1].length->unit == presentcss::Unit::Px);
}

TEST_CASE("unsupported selectors, properties, values, !important and external resources are reported and ignored") {
    core::DiagnosticsLog log;
    const auto sheet = presentcss::parseStyleSheet(
        "@import url(\"http://example.invalid/a.css\");\n"
        "div p { color: red }\n"
        "a:hover { color: red }\n"
        "p { float: left; color: notacolor; display: flex; color: blue !important; "
        "background-color: url(\"http://example.invalid/x.png\") }",
        log);
    REQUIRE(sheet.rules.size() == 1);
    REQUIRE(sheet.rules[0].declarations.size() == 1); // color: blue (important ignored)
    CHECK(sheet.rules[0].declarations[0].color == core::Color{0, 0, 255, 255});
    CHECK(count(log, "W-CSS-EXTERNAL-IGNORED") == 2);
    CHECK(count(log, "W-CSS-UNSUPPORTED-SELECTOR") == 2);
    CHECK(count(log, "W-CSS-UNSUPPORTED-PROPERTY") == 1);
    CHECK(count(log, "W-CSS-INVALID-VALUE") == 2);
    CHECK(count(log, "W-CSS-IMPORTANT-IGNORED") == 1);
}

TEST_CASE("cascade: inline > #id > .class > type, then source order; inheritance") {
    Styled s(U"<style>p { color: red } .c { color: green } #i { color: blue } p { color: yellow }</style>"
             U"<p>type</p><p class=\"c\">class</p><p id=\"i\" class=\"c\">id</p>"
             U"<p id=\"i\" style=\"color: #010203\">inline</p><div style=\"color: #0000ff\"><span>inherited</span></div>");
    const auto& b = s.body();
    CHECK(b.children[0]->style.color == core::Color{255, 255, 0, 255}); // later type rule wins
    CHECK(b.children[1]->style.color == core::Color{0, 128, 0, 255});
    CHECK(b.children[2]->style.color == core::Color{0, 0, 255, 255});
    CHECK(b.children[3]->style.color == core::Color{1, 2, 3, 255});
    CHECK(b.children[4]->children[0]->style.color == core::Color{0, 0, 255, 255}); // color inherits
    CHECK(b.children[4]->children[0]->style.background.a == 0);                    // background does not
}

TEST_CASE("font-family aliases map to packs; unresolved families use the active pack") {
    Styled s(U"<div style=\"font-family: 'Comic Sans MS', serif\">a<span>b</span></div>"
             U"<p style=\"font-family: Arial Narrow\">c</p><p style=\"font-family: Papyrus, Verdana\">d</p><p>e</p>");
    const auto& b = s.body();
    CHECK(b.children[0]->style.packId == "comic-style");
    CHECK(b.children[0]->style.fontExplicit);
    CHECK(b.children[0]->children[1]->style.packId == "comic-style"); // inherited
    CHECK(b.children[1]->style.packId == "arial-narrow-style");
    CHECK(b.children[2]->style.packId == "verdana-style");
    CHECK(b.children[3]->style.packId == "verdana-style");
    CHECK_FALSE(b.children[3]->style.fontExplicit);
}

TEST_CASE("default paragraph margins, em units and display:none") {
    Styled s(U"<p>x</p><p style=\"margin-top: 2em; margin-bottom: 3px\">y</p><div style=\"display: none\">z</div>");
    const auto& b = s.body();
    const int lh = s.reg.metrics("verdana-style").lineHeight;
    CHECK(b.children[0]->style.marginTopPx == lh);
    CHECK(b.children[0]->style.marginBottomPx == lh);
    CHECK(b.children[1]->style.marginTopPx == 2 * lh);
    CHECK(b.children[1]->style.marginBottomPx == 3);
    CHECK(b.children[2]->style.display == presentcss::DisplayValue::None);
    const auto blocks = html::toStyledBlocks(s.doc);
    for (const auto& blk : blocks)
        for (const auto& r : blk.runs) CHECK(r.text.find(U'z') == std::u32string::npos);
}
