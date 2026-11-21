#pragma once
// Logical-to-visual layout (data-model §8, research R-12/R-13).
//  * Full tier: bounded LRU of complete LayoutLines keyed by (lineRevision, params, packRevision).
//  * Metrics tier: one LineMetrics per logical line (exact once measured, otherwise estimated),
//    with a Fenwick tree of heights for O(log n) y <-> line mapping.
// Row and glyph positions are relative to their line / row, so a line's layout stays valid when other
// lines change.

#include <chrono>
#include <cstdint>
#include <list>
#include <string>
#include <unordered_map>
#include <vector>

#include "pxc/core/fenwick.hpp"
#include "pxc/document/text_document.hpp"
#include "pxc/fonts/font_registry.hpp"

namespace pxc::layout {

struct LayoutParams {
    std::string packId = std::string(fonts::kDefaultPackId);
    int scale = 1;
    int wrapWidth = 0; // device pixels; 0 = no wrap
    int tabCells = 4;  // tab stop = tabCells * spaceAdvance
    bool operator==(const LayoutParams&) const = default;
};

struct PositionedGlyph {
    std::size_t col = 0; // column within the logical line
    char32_t cp = 0;
    int x = 0;       // device px relative to the row start
    int advance = 0; // device px
};

struct VisualRow {
    std::size_t startCol = 0; // [startCol, endCol) columns of the logical line
    std::size_t endCol = 0;
    int y = 0;        // device px relative to the line top
    int height = 0;   // device px
    int width = 0;    // pen position after the last glyph
    int inkRight = 0; // rightmost inked pixel (exclusive)
    std::vector<PositionedGlyph> glyphs;
};

struct LayoutLine {
    std::uint64_t lineRevision = 0;
    std::vector<VisualRow> rows;
    int height = 0;
    int width = 0; // widest row
};

struct LineMetrics {
    std::uint32_t rows = 1;
    std::int32_t height = 0;
    bool exact = false;
};

struct CaretGeometry {
    std::size_t line = 0;
    std::size_t row = 0;
    int x = 0;    // device px relative to the row start (document x)
    int y = 0;    // document y of the row top
    int height = 0;
};

struct LayoutCounters {
    std::uint64_t linesLaidOut = 0;
    std::uint64_t rowsRecomputed = 0;
    std::uint64_t incrementalRelayouts = 0;
    void reset() noexcept { *this = {}; }
};

inline constexpr std::size_t kFullTierMaxLines = 4096;
inline constexpr std::size_t kFullTierMaxBytes = 32u << 20;

class LayoutEngine {
public:
    explicit LayoutEngine(const fonts::FontRegistry& registry);

    // Changing params invalidates all layout (font switch, wrap toggle, DPI, resize in wrap mode).
    void setParams(const LayoutParams& p);
    const LayoutParams& params() const noexcept { return params_; }

    // Synchronises the metrics tier with the document (call after setText / on attach).
    void reset(const document::TextDocument& doc);
    // Incremental update after an edit: splices metrics and re-wraps the edited line incrementally (R-13).
    // Returns the [firstRow, lastRow] visual rows of the edited line that changed (line-relative), or
    // {0, SIZE_MAX} when the whole line changed.
    std::pair<std::size_t, std::size_t> onEdit(const document::TextDocument& doc, const document::EditResult& e);
    void invalidateAll();

    // Full layout of a logical line (cached in the full tier).
    const LayoutLine& line(const document::TextDocument& doc, std::size_t line);
    LineMetrics metrics(std::size_t line) const;

    // Geometry in document pixels.
    std::int64_t lineTop(std::size_t line) const;
    std::int64_t totalHeight() const;
    std::size_t lineAtY(std::int64_t docY) const;
    int rowHeight() const noexcept { return rowHeight_; } // lineHeight * scale of the active pack
    int ascentPx() const noexcept { return ascent_; }       // ascent * scale
    int spaceAdvancePx() const noexcept { return spaceAdvance_; }
    int contentWidth() const noexcept { return maxWidth_; }

    CaretGeometry caretAt(const document::TextDocument& doc, std::size_t offset);
    // Offset nearest to document point (x relative to the left edge of text, y in document pixels).
    std::size_t hitTest(const document::TextDocument& doc, int docX, std::int64_t docY);
    // Offset in visual row `row` of `line` nearest to x.
    std::size_t offsetInRow(const document::TextDocument& doc, std::size_t line, std::size_t row, int x);

    // Progressive background measurement: computes exact metrics for unmeasured lines, nearest to
    // `focusLine` first, until `budget` is used. Returns false when every line is measured.
    bool layoutIdleSlice(const document::TextDocument& doc, std::chrono::microseconds budget,
                         std::size_t focusLine = 0);
    std::size_t unmeasuredLines() const noexcept { return unmeasured_; }

    std::size_t fullTierSize() const noexcept { return cache_.size(); }
    LayoutCounters& counters() noexcept { return counters_; }

private:
    LayoutLine build(const document::TextDocument& doc, std::size_t line) const;
    void wrapFrom(LayoutLine& out, const std::u32string& text, std::size_t startCol, int startY,
                  const LayoutLine* old, std::size_t editEndColNew, std::ptrdiff_t delta) const;
    void setMetrics(std::size_t line, const LayoutLine& l);
    LineMetrics estimate(const document::TextDocument& doc, std::size_t line) const;
    void rebuildHeights();
    const LayoutLine& store(LayoutLine&& l);
    int advanceOf(char32_t cp, int penX) const;

    const fonts::FontRegistry* registry_;
    LayoutParams params_;
    int rowHeight_ = 15;
    int ascent_ = 11;
    const document::TextDocument* doc_ = nullptr;
    int spaceAdvance_ = 4;
    int avgAdvance_ = 7;
    std::uint32_t packRevision_ = 0;

    std::vector<LineMetrics> metrics_;
    core::Fenwick heights_;
    std::size_t unmeasured_ = 0;
    std::size_t idleCursor_ = 0;
    int maxWidth_ = 0;

    struct Entry {
        LayoutLine line;
        std::size_t bytes = 0;
    };
    std::list<Entry> lru_;
    std::unordered_map<std::uint64_t, std::list<Entry>::iterator> cache_; // by line revision
    std::size_t cacheBytes_ = 0;
    mutable LayoutCounters counters_;
};

} // namespace pxc::layout
