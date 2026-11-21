#pragma once
// Block layout for the read-only HTML preview: block boxes of styled inline runs, wrapped greedily across runs
// with the same glyph resolution as the editor (FR-037).

#include <cstdint>
#include <string>
#include <vector>

#include "pxc/core/geometry.hpp"
#include "pxc/fonts/font_registry.hpp"

namespace pxc::layout {

struct StyledRun {
    std::u32string text; // whitespace already collapsed; '\n' = forced line break (<br>)
    core::Color fg{0, 0, 0, 255};
    core::Color bg{0, 0, 0, 0};
    std::string packId;
    double lineHeightMultiplier = 0; // 0 = normal (the pack's line height)
    double lineHeightPx = 0;         // > 0 overrides (at scale 1)
};

struct StyledBlock {
    std::vector<StyledRun> runs;
    core::Color background{0, 0, 0, 0};
    int marginTopPx = 0; // at scale 1
    int marginBottomPx = 0;
};

struct PreviewGlyph {
    std::size_t offset = 0; // into PreviewLayout::text
    char32_t cp = 0;
    std::uint16_t pack = 0; // index into PreviewLayout::packs
    int x = 0;
    int advance = 0;
    core::Color fg, bg;
};

struct PreviewRow {
    int y = 0;
    int height = 0;
    int baseline = 0; // relative to y
    std::vector<PreviewGlyph> glyphs;
};

struct PreviewBackground {
    core::Rect rect;
    core::Color color;
};

struct PreviewLayout {
    std::vector<std::string> packs;
    std::vector<PreviewRow> rows;
    std::vector<PreviewBackground> backgrounds; // block and inline backgrounds, in paint order
    std::u32string text;                        // plain text of the preview (selection / copy)
    int width = 0;
    int height = 0;
};

PreviewLayout layoutBlocks(const std::vector<StyledBlock>& blocks, const fonts::FontRegistry& registry, int width,
                           int scale, int marginLeft = 4);

// Offset in PreviewLayout::text nearest to a document point.
std::size_t previewHitTest(const PreviewLayout& l, int x, int y);

} // namespace pxc::layout
