#pragma once
// Immutable parse result of one glyph CSS file (data-model §1, contracts/glyph-css-schema.md).

#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "pxc/core/diagnostic.hpp"
#include "pxc/core/geometry.hpp"

namespace pxc::glyphcss {

inline constexpr const char* kGlyphSchema = "pixelcss-glyph/1";

enum class CellKind : std::uint8_t {
    Transparent,     // no ink; never overwrites the background
    Literal,         // literal #RRGGBB / #RRGGBBAA colour
    CurrentColor,    // foreground substitution
    BackgroundColor, // background substitution
};

struct Cell {
    CellKind kind = CellKind::Transparent;
    core::Color rgba{0, 0, 0, 0}; // meaningful for Literal only
    bool operator==(const Cell&) const = default;
};

struct GlyphDefinition {
    std::string schema;
    char32_t codePoint = 0;
    std::string family;
    std::string style = "regular";
    int width = 0;
    int height = 0;
    int baseline = 0; // rows above the baseline
    int advance = 0;
    int bearingX = 0;
    std::vector<Cell> cells; // width*height, row-major
    std::vector<std::pair<std::string, std::string>> metadata; // --meta-<name>, source order
    std::uint64_t contentHash = 0;
    std::filesystem::path sourcePath;
    std::vector<core::Diagnostic> warnings; // non-fatal findings (e.g. W-GLYPH-UNKNOWN-PROP)

    const Cell& cellAt(int col, int row) const {
        return cells[static_cast<std::size_t>(row) * static_cast<std::size_t>(width) + static_cast<std::size_t>(col)];
    }
    bool hasInk() const noexcept {
        for (const Cell& c : cells)
            if (c.kind != CellKind::Transparent) return true;
        return false;
    }
};

// FNV-1a 64 over the text with CRLF normalised to LF (glyph revision identity).
std::uint64_t contentHash(std::string_view text) noexcept;

// Canonical token for a cell: ".", "@", "~" or "#RRGGBBAA".
std::string cellToken(const Cell& c);

} // namespace pxc::glyphcss
