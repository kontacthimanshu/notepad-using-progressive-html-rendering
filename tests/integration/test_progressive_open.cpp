// Progressive first paint and idle layout of a 100k-character document (US4, FR-018, R-12).
#include <doctest.h>

#include <chrono>
#include <filesystem>

#include "pxc/core/unicode.hpp"
#include "support/editor_harness.hpp"
#include "support/fs_util.hpp"

using namespace pxc;
using namespace pxc::editor;
using test::EditorHarness;
namespace fs = std::filesystem;

TEST_CASE("first paint lays out only the visible lines; idle slices complete the rest" *
          doctest::description("leakcheck:off")) {
    const std::string bytes = test::readAll(fs::path(PXC_FIXTURES_DIR) / "perf/100k.txt");
    REQUIRE(bytes.size() >= 100000);
    const auto text = *core::decodeUtf8(bytes, core::InvalidPolicy::Replace);

    EditorHarness h(640, 480, true);
    h.editor->loadText(text);
    h.layout->counters().reset();
    const auto st = h.paint();
    const int visibleRows = 480 / h.layout->rowHeight() + 1;
    CHECK(h.layout->counters().linesLaidOut <= static_cast<std::uint64_t>(2 * visibleRows));
    CHECK(st.rasterizations <= 96); // at most the distinct glyphs on screen
    CHECK(h.layout->unmeasuredLines() > 0);

    const auto anchor = h.viewport.anchor;
    int slices = 0;
    while (h.layout->layoutIdleSlice(h.doc, std::chrono::milliseconds(4), anchor.line)) {
        ++slices;
        CHECK(h.viewport.anchor == anchor); // the visible content never jumps
        REQUIRE(slices < 10000);
    }
    CHECK(h.layout->unmeasuredLines() == 0);
    std::int64_t sum = 0;
    for (std::size_t i = 0; i < h.doc.lineCount(); ++i) {
        CHECK(h.layout->metrics(i).exact);
        sum += h.layout->metrics(i).height;
    }
    CHECK(h.layout->totalHeight() == sum);

    // Scrolling 50 rows reuses cached glyph rasters.
    const auto before = h.cache->stats();
    const auto oldY = h.viewport.scrollY(*h.layout);
    h.viewport.scrollBy(*h.layout, 50 * h.layout->rowHeight());
    h.updates->onScroll(oldY, h.viewport.scrollX);
    const auto st2 = h.paint();
    const auto after = h.cache->stats();
    CHECK(after.rasterizations - before.rasterizations <= 10);
    CHECK(st2.cacheHitRate >= 0.95);
}
