// Visual test: the custom editor surface (FR-033, FR-052 wrapping/clipping).
#include <doctest.h>

#include "support/editor_harness.hpp"
#include "support/golden.hpp"

using namespace pxc;
using namespace pxc::editor;
using test::EditorHarness;

TEST_CASE("3-line document with caret") {
    EditorHarness h(320, 120, false);
    h.type(U"The quick brown fox\njumps over the lazy dog.\n0123456789 !?");
    h.paint();
    CHECK_GOLDEN(h.fb, "editor/basic.bmp");
}

TEST_CASE("wrapped paragraph in a narrow surface") {
    EditorHarness h(160, 120, true);
    h.type(U"Word wrap breaks this sentence across several visual rows.");
    h.paint();
    CHECK_GOLDEN(h.fb, "editor/wrapped.bmp");
}

TEST_CASE("horizontal scroll clips glyphs at the left edge") {
    EditorHarness h(200, 60, false);
    h.type(U"A long line that scrolls horizontally past the left edge");
    h.viewport.scrollX = 37;
    h.updates->invalidateAll();
    h.paint();
    CHECK_GOLDEN(h.fb, "editor/clipped.bmp");
}

TEST_CASE("selection is drawn with inverted glyph colours") {
    EditorHarness h(320, 60, false);
    h.type(U"select some words here");
    h.move(MoveKind::Home);
    for (int i = 0; i < 7; ++i) h.move(MoveKind::Right);
    for (int i = 0; i < 10; ++i) h.move(MoveKind::Right, true);
    h.paint();
    CHECK_GOLDEN(h.fb, "editor/selection.bmp");
    // The selection fill colour is present.
    bool hasFill = false;
    for (int y = 0; y < h.fb.height() && !hasFill; ++y)
        for (int x = 0; x < h.fb.width(); ++x)
            if (h.fb.color(x, y) == h.updates->theme().selectionFill) { hasFill = true; break; }
    CHECK(hasFill);
}

TEST_CASE("a selection starting on line 2 does not paint line 1's line break") {
    EditorHarness h(320, 60, false);
    h.type(U"first\nsecond");
    h.move(MoveKind::Home, true); // selects "second" only
    REQUIRE(h.editor->state().selStart() == 6);
    h.paint();
    const auto line1 = h.layout->line(h.doc, 0);
    const int y = h.layout->rowHeight() / 2;
    // Just right of "first" (where a selected line break would be drawn) must stay background.
    for (int dx = 1; dx < h.layout->spaceAdvancePx(); ++dx)
        CHECK(h.fb.color(h.viewport.toViewX(line1.width) + dx, y) == h.updates->theme().background);
    // ...while line 2 is highlighted.
    CHECK(h.fb.color(h.viewport.toViewX(1), h.layout->rowHeight() + 1) == h.updates->theme().selectionFill);
}
