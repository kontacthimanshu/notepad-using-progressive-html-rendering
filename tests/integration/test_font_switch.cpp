// US3 independent test: switching fonts is presentation-only (FR-026, SC-004).
#include <doctest.h>

#include "support/editor_harness.hpp"
#include "support/golden.hpp"

using namespace pxc;
using namespace pxc::editor;
using test::EditorHarness;

TEST_CASE("switching among the three packs preserves text, caret and selection; layout changes") {
    EditorHarness h(260, 140, true);
    const std::u32string text =
        U"Aa0!? Hello World\nThis long line wraps differently in every one of the three pixel font packs. "
        U"It is long enough that the condensed pack needs visibly fewer rows than the wide one.";
    h.type(text);
    // Select "Hello" and leave the caret after it.
    h.editor->handle(cmd::Move{MoveKind::DocStart});
    for (int i = 0; i < 6; ++i) h.move(MoveKind::Right);
    for (int i = 0; i < 5; ++i) h.move(MoveKind::Right, true);
    const auto caret = h.caret(), anchor = h.anchor();
    REQUIRE(h.doc.text(h.editor->state().selStart(), 5) == U"Hello");
    const auto revision = h.doc.revision();

    std::vector<std::uint32_t> rows;
    for (const char* pack : {"arial-narrow-style", "comic-style", "verdana-style"}) {
        CAPTURE(pack);
        CHECK(h.editor->handle(cmd::SetFont{pack}));
        CHECK(h.editor->state().activePackId == pack);
        CHECK(h.doc.text() == text);
        CHECK(h.doc.revision() == revision); // the document is never touched
        CHECK(h.caret() == caret);
        CHECK(h.anchor() == anchor);
        CHECK(h.editor->state().preferredX == std::nullopt); // recomputed from the caret offset
        (void)h.layout->line(h.doc, 1);
        rows.push_back(h.layout->metrics(1).rows);
        h.paint();
        CHECK_GOLDEN(h.fb, std::string("font_switch/") + pack + ".bmp");
    }
    // Condensed Arial Narrow-style needs fewer rows than the wider Verdana-style.
    CHECK(rows[0] < rows[2]);
    CHECK(h.doc.modified()); // typed text stays modified: font switching neither saves nor resets anything
}

TEST_CASE("switching to the active font is a no-op") {
    EditorHarness h;
    CHECK_FALSE(h.editor->handle(cmd::SetFont{"verdana-style"}));
}
