// Contract test for TextDocument (data-model §6).
#include <doctest.h>

#include <chrono>

#include "pxc/core/clock.hpp"
#include "pxc/document/text_document.hpp"

using namespace pxc;
using namespace pxc::document;
using namespace std::chrono_literals;

TEST_CASE("insert and erase at start, middle, end and across lines keep the line index consistent") {
    TextDocument d;
    d.setText(U"hello\nworld");
    CHECK(d.length() == 11);
    CHECK(d.lineCount() == 2);
    CHECK(d.lineStart(1) == 6);
    CHECK(d.lineLength(0) == 5);
    CHECK(d.toLineCol(7) == LineCol{1, 1});
    CHECK(d.toOffset({1, 99}) == 11);

    d.insert(0, U">");
    CHECK(d.text() == U">hello\nworld");
    d.insert(6, U" big");
    CHECK(d.text() == U">hello big\nworld");
    d.insert(d.length(), U"!\nend");
    CHECK(d.text() == U">hello big\nworld!\nend");
    CHECK(d.lineCount() == 3);
    CHECK(d.lineText(2) == U"end");
    d.erase(9, 3); // "g\nw" across the line break
    CHECK(d.text() == U">hello biorld!\nend");
    CHECK(d.lineCount() == 2);
    CHECK(d.lineStart(1) == 15);
    for (std::size_t off = 0; off <= d.length(); ++off) CHECK(d.toOffset(d.toLineCol(off)) == off);
}

TEST_CASE("revision increments on every change; lineRevision changes only for affected lines") {
    TextDocument d;
    d.setText(U"a\nb\nc\nd");
    const auto r0 = d.revision();
    const auto l0 = d.lineRevision(0), l2 = d.lineRevision(2), l3 = d.lineRevision(3);
    d.insert(d.lineStart(1), U"x");
    CHECK(d.revision() > r0);
    CHECK(d.lineRevision(0) == l0);
    CHECK(d.lineRevision(2) == l2);
    CHECK(d.lineRevision(3) == l3);
    const auto l1 = d.lineRevision(1);
    auto e = d.insert(d.lineStart(1), U"\n"); // splits line 1: lines below shift but keep their revisions
    CHECK(e.firstLine == 1);
    CHECK(e.oldLineCount == 1);
    CHECK(e.newLineCount == 2);
    CHECK(d.lineRevision(3) == l2);
    CHECK(d.lineRevision(4) == l3);
    CHECK(d.lineRevision(1) != l1);
}

TEST_CASE("carriage returns are never stored") {
    TextDocument d;
    d.insert(0, U"a\r\nb\rc");
    CHECK(d.text() == U"a\nb\nc");
    d.setText(U"x\r\ny");
    CHECK(d.text() == U"x\ny");
}

TEST_CASE("undo and redo restore text and caret/selection snapshots") {
    TextDocument d;
    d.setText(U"abc");
    d.insert(3, U"def", {3, 3}, {6, 6});
    d.breakUndoGroup();
    d.erase(0, 2, {0, 2}, {0, 0});
    CHECK(d.text() == U"cdef");
    EditorSnapshot s;
    std::vector<EditResult> edits;
    REQUIRE(d.undo(&s, &edits));
    CHECK(d.text() == U"abcdef");
    CHECK(s == EditorSnapshot{0, 2});
    CHECK(edits.size() == 1);
    REQUIRE(d.undo(&s));
    CHECK(d.text() == U"abc");
    CHECK(s == EditorSnapshot{3, 3});
    CHECK_FALSE(d.undo(&s));
    REQUIRE(d.redo(&s));
    CHECK(d.text() == U"abcdef");
    CHECK(s == EditorSnapshot{6, 6});
    REQUIRE(d.redo(&s));
    CHECK(d.text() == U"cdef");
    CHECK_FALSE(d.redo());
}

TEST_CASE("typing coalesces within 1 s; whitespace-to-word boundary and pauses split groups") {
    core::ManualClock clock;
    TextDocument d(&clock);
    std::size_t pos = 0;
    for (char32_t c : std::u32string(U"hello world")) {
        d.insert(pos, std::u32string(1, c));
        ++pos;
        clock.advance(100ms);
    }
    CHECK(d.undoGroupCount() == 2); // "hello " and "world"
    clock.advance(2s);
    d.insert(pos, U"!");
    CHECK(d.undoGroupCount() == 3); // pause > 1 s
    REQUIRE(d.undo());
    CHECK(d.text() == U"hello world");
    REQUIRE(d.undo());
    CHECK(d.text() == U"hello ");
    // Consecutive backspaces coalesce too.
    TextDocument b(&clock);
    b.setText(U"abcdef");
    for (std::size_t i = 6; i > 3; --i) {
        b.erase(i - 1, 1);
        clock.advance(50ms);
    }
    CHECK(b.text() == U"abc");
    CHECK(b.undoGroupCount() == 1);
}

TEST_CASE("undo history is capped at 10000 groups" * doctest::description("leakcheck:off")) {
    TextDocument d;
    for (std::size_t i = 0; i < kMaxUndoGroups + 50; ++i) {
        d.breakUndoGroup();
        d.insert(d.length(), U"x");
    }
    CHECK(d.undoGroupCount() == kMaxUndoGroups);
}

TEST_CASE("modified flag follows saves and undo back to the saved point") {
    TextDocument d;
    d.setText(U"abc");
    CHECK_FALSE(d.modified());
    d.insert(3, U"d");
    CHECK(d.modified());
    d.markSaved();
    CHECK_FALSE(d.modified());
    d.breakUndoGroup();
    d.insert(4, U"e");
    CHECK(d.modified());
    REQUIRE(d.undo());
    CHECK_FALSE(d.modified());
    REQUIRE(d.undo());
    CHECK(d.modified());
}

TEST_CASE("non-BMP code points count as one offset") {
    TextDocument d;
    d.setText(U"a\U0001F600b");
    CHECK(d.length() == 3);
    CHECK(d.at(1) == U'\U0001F600');
    d.erase(1, 1);
    CHECK(d.text() == U"ab");
}
