// Contract test for EditorController (US2 acceptance scenarios 1–3).
#include <doctest.h>

#include "support/editor_harness.hpp"

using namespace pxc;
using namespace pxc::editor;
using test::EditorHarness;

TEST_CASE("typing printable characters and Enter updates the buffer and caret") {
    EditorHarness h;
    h.type(U"Hello");
    h.editor->handle(cmd::Enter{});
    h.type(U"World");
    CHECK(h.doc.text() == U"Hello\nWorld");
    CHECK(h.caret() == 11);
    CHECK(h.doc.modified());
}

TEST_CASE("backspace and delete at boundaries and across newlines") {
    EditorHarness h;
    h.editor->handle(cmd::Backspace{}); // at offset 0: no-op
    CHECK(h.doc.length() == 0);
    h.type(U"ab");
    h.editor->handle(cmd::Enter{});
    h.type(U"cd");
    h.editor->handle(cmd::Delete{}); // at end: no-op
    CHECK(h.doc.text() == U"ab\ncd");
    h.move(MoveKind::Home);
    h.editor->handle(cmd::Backspace{}); // joins lines
    CHECK(h.doc.text() == U"abcd");
    CHECK(h.caret() == 2);
    h.editor->handle(cmd::Enter{});
    h.move(MoveKind::Left);
    h.editor->handle(cmd::Delete{}); // deletes the newline
    CHECK(h.doc.text() == U"abcd");
}

TEST_CASE("arrows, Home/End, document start/end, word moves and paging") {
    EditorHarness h;
    h.type(U"one two three\nfour five\nsix");
    h.move(MoveKind::DocStart);
    CHECK(h.caret() == 0);
    h.move(MoveKind::WordRight);
    CHECK(h.caret() == 4);
    h.move(MoveKind::WordRight);
    CHECK(h.caret() == 8);
    h.move(MoveKind::WordLeft);
    CHECK(h.caret() == 4);
    h.move(MoveKind::End);
    CHECK(h.caret() == 13);
    h.move(MoveKind::Right);
    CHECK(h.caret() == 14); // start of line 2
    h.move(MoveKind::Down);
    CHECK(h.doc.toLineCol(h.caret()).line == 2);
    h.move(MoveKind::Up);
    h.move(MoveKind::Up);
    CHECK(h.doc.toLineCol(h.caret()).line == 0);
    h.move(MoveKind::DocEnd);
    CHECK(h.caret() == h.doc.length());
    h.move(MoveKind::PageUp);
    CHECK(h.caret() == 0);
    h.move(MoveKind::PageDown); // keeps the column (0) and lands on the last line
    CHECK(h.doc.toLineCol(h.caret()).line == 2);
    CHECK(h.caret() == h.doc.lineStart(2));
    h.move(MoveKind::End);
    CHECK(h.caret() == h.doc.length());
    h.move(MoveKind::Home);
    CHECK(h.caret() == h.doc.lineStart(2));
}

TEST_CASE("vertical moves keep the preferred x column") {
    EditorHarness h;
    h.type(U"abcdefgh\nab\nabcdefgh");
    h.move(MoveKind::DocStart);
    for (int i = 0; i < 6; ++i) h.move(MoveKind::Right);
    h.move(MoveKind::Down); // clamps to end of "ab"
    CHECK(h.caret() == h.doc.lineStart(1) + 2);
    h.move(MoveKind::Down); // returns to column 6
    CHECK(h.caret() == h.doc.lineStart(2) + 6);
}

TEST_CASE("shift-extended selection, then typing replaces the selection") {
    EditorHarness h;
    h.type(U"hello world");
    h.move(MoveKind::Home);
    for (int i = 0; i < 5; ++i) h.move(MoveKind::Right, true);
    CHECK(h.editor->state().selStart() == 0);
    CHECK(h.editor->state().selEnd() == 5);
    h.type(U"HELLO");
    CHECK(h.doc.text() == U"HELLO world");
    CHECK_FALSE(h.editor->state().hasSelection());
    h.editor->handle(cmd::SelectAll{});
    CHECK(h.editor->state().selEnd() == h.doc.length());
}

TEST_CASE("cut, copy and paste exchange Unicode text, including characters without glyphs") {
    EditorHarness h;
    h.type(U"xé漢\U0001F600y");
    h.editor->handle(cmd::SelectAll{});
    h.editor->handle(cmd::Copy{});
    REQUIRE(h.clipboard.content);
    CHECK(*h.clipboard.content == U"xé漢\U0001F600y");
    h.editor->handle(cmd::Cut{});
    CHECK(h.doc.length() == 0);
    h.editor->handle(cmd::Paste{});
    CHECK(h.doc.text() == U"xé漢\U0001F600y"); // FR-031: preserved even without glyphs
    h.clipboard.content = U"a\r\nb\rc";
    h.editor->handle(cmd::Paste{});
    CHECK(h.doc.text() == U"xé漢\U0001F600ya\nb\nc");
}

TEST_CASE("undo and redo restore caret and selection") {
    EditorHarness h;
    h.type(U"abc");
    h.move(MoveKind::Home);
    h.move(MoveKind::End, true);
    h.editor->handle(cmd::Delete{});
    CHECK(h.doc.text().empty());
    h.editor->handle(cmd::Undo{});
    CHECK(h.doc.text() == U"abc");
    CHECK(h.editor->state().selStart() == 0);
    CHECK(h.editor->state().selEnd() == 3);
    h.editor->handle(cmd::Redo{});
    CHECK(h.doc.text().empty());
    CHECK(h.caret() == 0);
}

TEST_CASE("modified state transitions") {
    EditorHarness h;
    CHECK_FALSE(h.doc.modified());
    h.type(U"a");
    CHECK(h.doc.modified());
    h.doc.markSaved();
    CHECK_FALSE(h.doc.modified());
    h.editor->handle(cmd::Backspace{});
    CHECK(h.doc.modified());
    h.editor->handle(cmd::Undo{});
    CHECK_FALSE(h.doc.modified());
}

TEST_CASE("read-only mode rejects edits but allows selection and copy") {
    EditorHarness h;
    h.editor->loadText(U"preview text");
    h.editor->setReadOnly(true);
    h.type(U"x");
    h.editor->handle(cmd::Backspace{});
    h.editor->handle(cmd::Paste{});
    CHECK(h.doc.text() == U"preview text");
    h.editor->handle(cmd::SelectAll{});
    h.editor->handle(cmd::Copy{});
    REQUIRE(h.clipboard.content);
    CHECK(*h.clipboard.content == U"preview text");
}
