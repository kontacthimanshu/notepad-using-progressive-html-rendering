#include "pxc/layout/viewport.hpp"

#include <algorithm>

namespace pxc::layout {

void Viewport::setScrollY(LayoutEngine& le, std::int64_t y) {
    const std::int64_t maxY = std::max<std::int64_t>(0, le.totalHeight() - clientHeight);
    y = std::clamp<std::int64_t>(y, 0, maxY);
    const std::size_t line = le.lineAtY(y);
    anchor.line = line;
    anchor.pixelOffset = static_cast<int>(y - le.lineTop(line));
}

void Viewport::clampScroll(LayoutEngine& le) { setScrollY(le, scrollY(le)); }

void Viewport::scrollToMakeVisible(LayoutEngine& le, const CaretGeometry& caret) {
    const std::int64_t top = scrollY(le);
    if (caret.y < top) setScrollY(le, caret.y);
    else if (caret.y + caret.height > top + clientHeight) setScrollY(le, caret.y + caret.height - clientHeight);
    else clampScroll(le);
    if (le.params().wrapWidth > 0) {
        scrollX = 0;
        return;
    }
    const int visibleW = std::max(1, clientWidth - 2 * marginLeft);
    if (caret.x < scrollX) scrollX = std::max(0, caret.x - visibleW / 4);
    else if (caret.x + 2 > scrollX + visibleW) scrollX = caret.x + 2 - visibleW + visibleW / 4;
}

std::vector<VisibleRow> Viewport::visibleRows(LayoutEngine& le, const document::TextDocument& doc,
                                              int overscanRows) const {
    std::vector<VisibleRow> out;
    const int limit = clientHeight + overscanRows * le.rowHeight();
    std::size_t line = std::min(anchor.line, doc.lineCount() - 1);
    int y = -anchor.pixelOffset;
    while (line < doc.lineCount() && y < limit) {
        const LayoutLine& l = le.line(doc, line);
        for (std::size_t r = 0; r < l.rows.size(); ++r) {
            const int top = y + l.rows[r].y;
            if (top + l.rows[r].height > 0 && top < limit) out.push_back({line, r, top});
        }
        y += l.height;
        ++line;
    }
    return out;
}

} // namespace pxc::layout
