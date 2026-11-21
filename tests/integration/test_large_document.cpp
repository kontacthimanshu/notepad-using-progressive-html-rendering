// A 10,000,000-character document opens progressively with bounded memory (FR-018, R-12, R-17).
#include <doctest.h>

#include <windows.h>
#include <psapi.h>

#include <chrono>

#include "support/editor_harness.hpp"

using namespace pxc;

TEST_CASE("10M characters: first paint rasterizes only on-screen glyphs; full tier stays bounded" *
          doctest::description("leakcheck:off") * doctest::timeout(60)) {
    std::u32string text;
    text.reserve(10'000'000 + 128);
    static const char32_t* words[] = {U"pixel ", U"glyph ", U"cascade ", U"layout ", U"viewport ", U"caret ", U"wrap "};
    std::size_t line = 0;
    while (text.size() < 10'000'000) {
        for (int w = 0; w < 8; ++w) text += words[(line + static_cast<std::size_t>(w)) % 7];
        text += U'\n';
        ++line;
    }
    test::EditorHarness h(800, 600, true);
    h.editor->loadText(text);
    text.clear();
    text.shrink_to_fit();

    h.layout->counters().reset();
    const auto st = h.paint();
    CHECK(st.rasterizations <= 96);
    CHECK(h.layout->counters().linesLaidOut <= 200u);

    const auto start = std::chrono::steady_clock::now();
    while (h.layout->layoutIdleSlice(h.doc, std::chrono::milliseconds(50))) {
        REQUIRE(std::chrono::steady_clock::now() - start < std::chrono::seconds(50));
    }
    CHECK(h.layout->unmeasuredLines() == 0);
    CHECK(h.layout->fullTierSize() <= layout::kFullTierMaxLines);

    PROCESS_MEMORY_COUNTERS pmc{sizeof(pmc)};
    REQUIRE(GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)));
    MESSAGE("working set with a 10M-character document: " << pmc.WorkingSetSize / (1024 * 1024) << " MB");
    CHECK(pmc.WorkingSetSize <= 256ull * 1024 * 1024);
}
