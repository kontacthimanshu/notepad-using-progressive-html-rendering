#include <doctest.h>

#include "pxc/core/diagnostic.hpp"

using namespace pxc::core;

TEST_CASE("diagnostic formatting includes location and code") {
    const std::filesystem::path p = "fonts/x/glyphs/U+0041.css";
    const auto d = makeError("E-GLYPH-ROW-LENGTH", "row 4 has 6 cells, expected 7", &p, 12, 5);
    CHECK(format(d) == "fonts/x/glyphs/U+0041.css:12:5: error E-GLYPH-ROW-LENGTH: row 4 has 6 cells, expected 7");
    CHECK(format(makeWarning("W-X", "m")) == "warning W-X: m");
}

TEST_CASE("diagnostics log de-duplicates and counts") {
    DiagnosticsLog log;
    const std::filesystem::path p = "a.css";
    CHECK(log.add(makeError("E-A", "m", &p, 1, 1)));
    CHECK_FALSE(log.add(makeError("E-A", "m", &p, 1, 1)));
    CHECK(log.add(makeError("E-A", "m", &p, 2, 1)));
    CHECK(log.add(makeWarning("W-B", "w")));
    CHECK(log.size() == 3);
    CHECK(log.errorCount() == 2);
    CHECK(log.warningCount() == 1);
    log.clear();
    CHECK(log.size() == 0);
}
