// Contract test for LayoutEngine (FR-016, FR-017, FR-029, research R-12/R-13).
#include <doctest.h>

#include "pxc/fonts/font_registry.hpp"
#include "pxc/layout/layout_engine.hpp"

using namespace pxc;
using namespace pxc::layout;

namespace {
struct Env {
    core::DiagnosticsLog log;
    fonts::FontRegistry reg{log};
    document::TextDocument doc;
    LayoutEngine le{reg};
    Env() {
        reg.loadAll(PXC_FONTS_DIR);
        REQUIRE(reg.isUsable("verdana-style"));
    }
    int adv(char32_t c) const { return reg.resolve("verdana-style", c).def->advance; }
    void set(std::u32string_view t, int wrap = 0) {
        doc.setText(t);
        LayoutParams p;
        p.wrapWidth = wrap;
        le.setParams(p);
        le.reset(doc);
    }
};
} // namespace

TEST_CASE("no-wrap glyph x positions equal cumulative advances") {
    Env e;
    e.set(U"Hello, World");
    const auto& l = e.le.line(e.doc, 0);
    REQUIRE(l.rows.size() == 1);
    int x = 0;
    const std::u32string s = U"Hello, World";
    for (std::size_t i = 0; i < s.size(); ++i) {
        CAPTURE(i);
        CHECK(l.rows[0].glyphs[i].x == x);
        CHECK(l.rows[0].glyphs[i].col == i);
        x += e.adv(s[i]);
    }
    CHECK(l.rows[0].width == x);
}

TEST_CASE("wrap breaks after the last fitting space; long words break mid-word") {
    Env e;
    const int w = e.adv(U'a') * 10;
    e.set(U"aaaa bbbb cccc", w);
    const auto& l = e.le.line(e.doc, 0);
    REQUIRE(l.rows.size() == 2);
    CHECK(l.rows[0].startCol == 0);
    CHECK(l.rows[0].endCol == 10); // "aaaa bbbb " — the space stays at the end of the row
    CHECK(l.rows[1].startCol == 10);
    CHECK(l.rows[1].endCol == 14);
    CHECK(l.rows[1].y == l.rows[0].height);

    e.set(U"abcdefghijklmnopqrstuvwxyz", e.adv(U'a') * 5);
    const auto& m = e.le.line(e.doc, 0);
    CHECK(m.rows.size() >= 5);
    for (const auto& r : m.rows) CHECK(r.width <= e.adv(U'a') * 5 + e.adv(U'm'));
    CHECK(m.rows.back().endCol == 26);
}

TEST_CASE("explicit newlines start new logical lines; empty lines have one visual row") {
    Env e;
    e.set(U"a\n\nb");
    CHECK(e.doc.lineCount() == 3);
    CHECK(e.le.line(e.doc, 1).rows.size() == 1);
    CHECK(e.le.line(e.doc, 1).rows[0].glyphs.empty());
    CHECK(e.le.lineTop(2) == 2 * e.le.rowHeight());
}

TEST_CASE("tabs advance to the next multiple of 4 * space advance") {
    Env e;
    e.set(U"a\tb");
    const int stop = 4 * e.adv(U' ');
    const auto& r = e.le.line(e.doc, 0).rows[0];
    CHECK(r.glyphs[1].x == e.adv(U'a'));
    CHECK(r.glyphs[2].x == stop);
}

TEST_CASE("hitTest and caretAt round-trip for every offset") {
    Env e;
    e.set(U"The quick brown fox\njumps over\n\nthe lazy dog", e.adv(U'a') * 8);
    for (std::size_t off = 0; off <= e.doc.length(); ++off) {
        CAPTURE(off);
        const auto c = e.le.caretAt(e.doc, off);
        const std::size_t back = e.le.hitTest(e.doc, c.x, c.y + c.height / 2);
        // At a soft wrap boundary the caret of offset N sits at the start of the next row.
        CHECK(back == off);
    }
}

TEST_CASE("toggling wrap relays out correctly") {
    Env e;
    e.set(U"aaaa bbbb cccc dddd", e.adv(U'a') * 6);
    CHECK(e.le.line(e.doc, 0).rows.size() > 1);
    LayoutParams p = e.le.params();
    p.wrapWidth = 0;
    e.le.setParams(p);
    CHECK(e.le.line(e.doc, 0).rows.size() == 1);
    CHECK(e.le.totalHeight() == e.le.rowHeight());
}

TEST_CASE("a 10000-character line without wrap has the full scroll extent") {
    Env e;
    std::u32string s(10000, U'm');
    e.set(s);
    const auto& l = e.le.line(e.doc, 0);
    CHECK(l.width == 10000 * e.adv(U'm'));
    CHECK(e.le.contentWidth() >= l.width);
}

TEST_CASE("two-tier cache: at most 4096 full lines resident, metrics for all, identical re-layout") {
    Env e;
    std::u32string s;
    for (int i = 0; i < 5000; ++i) s += U"line number " + std::u32string(1, static_cast<char32_t>(U'a' + i % 26)) + U"\n";
    e.set(s);
    const auto first = e.le.line(e.doc, 0); // copy
    for (std::size_t i = 0; i < e.doc.lineCount(); ++i) (void)e.le.line(e.doc, i);
    CHECK(e.le.fullTierSize() <= kFullTierMaxLines);
    for (std::size_t i = 0; i < e.doc.lineCount(); ++i) CHECK(e.le.metrics(i).exact);
    const auto& again = e.le.line(e.doc, 0); // evicted earlier, re-laid out on demand
    REQUIRE(again.rows.size() == first.rows.size());
    CHECK(again.rows[0].glyphs.size() == first.rows[0].glyphs.size());
    CHECK(again.rows[0].width == first.rows[0].width);
}

TEST_CASE("intra-line incremental re-layout matches a full layout and recomputes few rows") {
    Env e;
    std::u32string s;
    for (int i = 0; s.size() < 100000; ++i) s += U"word" + std::u32string(1, static_cast<char32_t>(U'a' + i % 26)) + U" ";
    const int wrap = e.adv(U'w') * 40;
    e.set(s, wrap);
    const auto& before = e.le.line(e.doc, 0);
    REQUIRE(before.rows.size() > 100);
    e.le.counters().reset();
    const std::size_t at = 50000;
    auto res = e.doc.insert(at, U"X");
    e.le.onEdit(e.doc, res);
    const auto incremental = e.le.line(e.doc, 0);
    CHECK(e.le.counters().incrementalRelayouts == 1);
    CHECK(e.le.counters().rowsRecomputed < 10);

    // Reference: a fresh engine laying out the same text from scratch.
    LayoutEngine fresh(e.reg);
    fresh.setParams(e.le.params());
    fresh.reset(e.doc);
    const auto& full = fresh.line(e.doc, 0);
    REQUIRE(incremental.rows.size() == full.rows.size());
    for (std::size_t r = 0; r < full.rows.size(); ++r) {
        CAPTURE(r);
        CHECK(incremental.rows[r].startCol == full.rows[r].startCol);
        CHECK(incremental.rows[r].endCol == full.rows[r].endCol);
        CHECK(incremental.rows[r].width == full.rows[r].width);
    }

    // No-wrap: splice in place.
    LayoutParams p = e.le.params();
    p.wrapWidth = 0;
    e.le.setParams(p);
    (void)e.le.line(e.doc, 0);
    e.le.counters().reset();
    auto r2 = e.doc.insert(10, U"Y");
    e.le.onEdit(e.doc, r2);
    const auto& nw = e.le.line(e.doc, 0);
    CHECK(e.le.counters().incrementalRelayouts == 1);
    CHECK(nw.rows[0].glyphs[10].cp == U'Y');
    CHECK(nw.rows[0].glyphs[11].x == nw.rows[0].glyphs[10].x + e.adv(U'Y'));
}
