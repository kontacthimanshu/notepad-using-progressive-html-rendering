// Incremental repainting (US4, FR-019/FR-020/FR-021, SC-006).
#include <doctest.h>

#include <random>

#include "support/editor_harness.hpp"

using namespace pxc;
using namespace pxc::editor;
using test::EditorHarness;

namespace {

std::u32string paragraph(int lines) {
    std::u32string t;
    for (int i = 0; i < lines; ++i) {
        t += U"Line " + std::u32string(1, static_cast<char32_t>(U'A' + i % 26)) + U": short stable text";
        if (i + 1 < lines) t += U'\n';
    }
    return t;
}

// Paints everything from scratch into a second framebuffer with the same state.
render::Framebuffer fullRepaint(EditorHarness& h) {
    render::Framebuffer ref(h.fb.width(), h.fb.height());
    h.updates->invalidateAll();
    h.updates->paint(ref, h.editor->paintState(), nullptr);
    return ref;
}

bool rectInside(const core::Rect& r, const core::Rect& outer) { return outer.intersect(r) == r; }

} // namespace

TEST_CASE("(a) a stable keystroke repaints only the edited row and the caret") {
    EditorHarness h(640, 480, false);
    h.editor->loadText(paragraph(20));
    h.paint();
    // Caret at the end of line 3 (a stable line).
    h.editor->handle(cmd::Move{MoveKind::DocStart});
    for (int i = 0; i < 3; ++i) h.move(MoveKind::Down);
    h.move(MoveKind::End);
    h.paint();
    h.layout->counters().reset();
    h.type(U"x");
    const auto dirty = h.updates->dirty();
    REQUIRE_FALSE(dirty.full());
    const int rowH = h.layout->rowHeight();
    const core::Rect editedRow{0, 3 * rowH, h.fb.width(), rowH};
    for (const auto& r : dirty.rects()) CHECK(rectInside(r, editedRow));
    const auto st = h.paint();
    CHECK(h.layout->counters().linesLaidOut == 1);
    CHECK(st.rasterizations <= 1);
    CHECK_FALSE(st.fullRepaint);
    CHECK(st.dirtyPixelArea <= static_cast<std::uint64_t>(2 * rowH * h.fb.width()));
    CHECK(h.fb == fullRepaint(h));
}

TEST_CASE("(b) an insertion that changes wrapping dirties the edited row through the viewport bottom") {
    EditorHarness h(200, 300, true);
    h.editor->loadText(U"aaaa bbbb cccc\nsecond line\nthird line");
    h.paint();
    h.editor->handle(cmd::Move{MoveKind::DocStart});
    h.move(MoveKind::End);
    const auto rowsBefore = h.layout->metrics(0).rows;
    h.type(U" and some more words that force another wrapped row");
    REQUIRE(h.layout->metrics(0).rows > rowsBefore);
    const auto box = h.updates->dirty().boundingBox();
    CHECK((h.updates->dirty().full() || box.bottom() >= h.fb.height()));
    h.paint();
    CHECK(h.fb == fullRepaint(h));
}

TEST_CASE("(c) inserting a newline shifts and repaints the following rows") {
    EditorHarness h(640, 300, false);
    h.editor->loadText(paragraph(8));
    h.paint();
    h.editor->handle(cmd::Move{MoveKind::DocStart});
    h.move(MoveKind::Down);
    h.move(MoveKind::End);
    h.editor->handle(cmd::Enter{});
    const auto box = h.updates->dirty().boundingBox();
    CHECK(box.y <= h.layout->rowHeight());
    CHECK(box.bottom() >= h.fb.height());
    h.paint();
    CHECK(h.fb == fullRepaint(h));
}

TEST_CASE("(d) font change, resize and wrap toggle mark the full viewport") {
    EditorHarness h(320, 200, true);
    h.editor->loadText(paragraph(5));
    h.paint();
    h.editor->handle(cmd::SetFont{"comic-style"});
    CHECK(h.updates->dirty().full());
    h.paint();
    h.editor->handle(cmd::ToggleWrap{});
    CHECK(h.updates->dirty().full());
    h.paint();
    h.updates->onResize(300, 180);
    CHECK(h.updates->dirty().full());
}

TEST_CASE("(e) oracle: 200 random edits, scrolls and font switches equal a full repaint" *
          doctest::description("leakcheck:off")) {
    EditorHarness h(360, 240, true);
    h.editor->loadText(paragraph(30));
    h.paint();
    std::mt19937 rng(424242);
    const char* packs[] = {"verdana-style", "arial-narrow-style", "comic-style"};
    const MoveKind moves[] = {MoveKind::Left, MoveKind::Right, MoveKind::Up, MoveKind::Down, MoveKind::Home,
                              MoveKind::End, MoveKind::WordLeft, MoveKind::WordRight, MoveKind::PageDown, MoveKind::PageUp};
    for (int step = 0; step < 200; ++step) {
        CAPTURE(step);
        switch (rng() % 9) {
        case 0: case 1: h.type(std::u32string(1, static_cast<char32_t>(U'a' + rng() % 26))); break;
        case 2: h.editor->handle(cmd::Enter{}); break;
        case 3: h.editor->handle(cmd::Backspace{}); break;
        case 4: h.editor->handle(cmd::Delete{}); break;
        case 5: case 6: h.move(moves[rng() % std::size(moves)], rng() % 4 == 0); break;
        case 7: {
            const auto oldY = h.viewport.scrollY(*h.layout);
            h.viewport.scrollBy(*h.layout, static_cast<int>(rng() % 120) - 60);
            h.updates->onScroll(oldY, h.viewport.scrollX);
            break;
        }
        default:
            if (rng() % 4 == 0) h.editor->handle(cmd::SetFont{packs[rng() % 3]});
            else h.type(U" ");
            break;
        }
        h.paint();
        const auto ref = fullRepaint(h);
        REQUIRE(h.fb == ref);
    }
}

TEST_CASE("(f) long line: one keystroke re-wraps only to the resync point" * doctest::description("leakcheck:off")) {
    for (bool wrap : {true, false}) {
        CAPTURE(wrap);
        EditorHarness h(480, 320, wrap);
        std::u32string line;
        for (int i = 0; line.size() < 100000; ++i) line += U"word" + std::u32string(1, static_cast<char32_t>(U'a' + i % 26)) + U" ";
        h.editor->loadText(line);
        h.paint();
        // Caret near the start so the edit is visible.
        h.editor->handle(cmd::Move{MoveKind::DocStart});
        for (int i = 0; i < 12; ++i) h.move(MoveKind::Right);
        h.paint();
        h.layout->counters().reset();
        h.type(U"Q");
        CHECK(h.layout->counters().incrementalRelayouts == 1);
        CHECK(h.layout->counters().rowsRecomputed < 10);
        const auto st = h.paint();
        CHECK(st.rasterizations <= 1);
        CHECK(st.dirtyPixelArea <= static_cast<std::uint64_t>(3 * h.layout->rowHeight() * h.fb.width()));
        CHECK(h.fb == fullRepaint(h));
    }
}
