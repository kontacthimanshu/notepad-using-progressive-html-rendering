#pragma once
// Scroll state anchored to a logical line so refinement of estimated heights above the viewport never
// makes the visible content jump (research R-12).

#include <algorithm>
#include <cstdint>
#include <vector>

#include "pxc/core/geometry.hpp"
#include "pxc/layout/layout_engine.hpp"

namespace pxc::layout {

struct ViewportAnchor {
    std::size_t line = 0;
    int pixelOffset = 0; // device px from the top of `line` to the top of the viewport (>= 0)
    bool operator==(const ViewportAnchor&) const = default;
};

struct VisibleRow {
    std::size_t line = 0;
    std::size_t row = 0;
    int y = 0; // viewport-relative device px of the row top
};

class Viewport {
public:
    int clientWidth = 0;
    int clientHeight = 0;
    int scrollX = 0;
    int marginLeft = 4; // device px of left padding inside the surface
    ViewportAnchor anchor;

    // Document y of the viewport top.
    std::int64_t scrollY(const LayoutEngine& le) const { return le.lineTop(anchor.line) + anchor.pixelOffset; }
    void setScrollY(LayoutEngine& le, std::int64_t y);
    void scrollBy(LayoutEngine& le, std::int64_t dy) { setScrollY(le, scrollY(le) + dy); }
    // Scrolls minimally so the caret rectangle is fully visible.
    void scrollToMakeVisible(LayoutEngine& le, const CaretGeometry& caret);
    void clampScroll(LayoutEngine& le);

    // Visible rows top to bottom (laying out lines as needed), plus `overscanRows` below.
    std::vector<VisibleRow> visibleRows(LayoutEngine& le, const document::TextDocument& doc, int overscanRows = 0) const;
    // Number of whole rows that fit.
    int pageRows(const LayoutEngine& le) const { return le.rowHeight() > 0 ? std::max(1, clientHeight / le.rowHeight()) : 1; }

    // Viewport <-> document coordinates.
    int toViewX(int docX) const noexcept { return docX - scrollX + marginLeft; }
    int toDocX(int viewX) const noexcept { return viewX + scrollX - marginLeft; }
};

} // namespace pxc::layout
