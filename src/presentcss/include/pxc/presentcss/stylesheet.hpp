#pragma once
// Presentation-CSS subset for the HTML preview (contracts/html-subset.md). A parser separate from the glyph and
// manifest dialects; only the tokenizer is shared (FR-041).

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "pxc/core/diagnostic.hpp"
#include "pxc/core/geometry.hpp"

namespace pxc::presentcss {

enum class Property { Color, BackgroundColor, FontFamily, MarginTop, MarginBottom, LineHeight, Display };
enum class Unit { None, Px, Em };
enum class DisplayValue { Block, Inline, None };

struct Length {
    double value = 0;
    Unit unit = Unit::Px;
};

struct Declaration {
    Property property = Property::Color;
    std::optional<core::Color> color;  // Color / BackgroundColor (transparent has alpha 0)
    std::vector<std::string> families; // FontFamily, in priority order
    std::optional<Length> length;      // margins (px/em); line-height (px or unit-less multiplier)
    bool lineHeightNormal = false;
    DisplayValue display = DisplayValue::Block;
};

struct Selector {
    std::string tag; // lower-case, empty = any
    std::string cls;
    std::string id;
    int specificity() const noexcept { return (id.empty() ? 0 : 100) + (cls.empty() ? 0 : 10) + (tag.empty() ? 0 : 1); }
    bool matches(std::string_view tag, std::string_view id, const std::vector<std::string>& classes) const;
};

struct Rule {
    std::vector<Selector> selectors;
    std::vector<Declaration> declarations;
    std::size_t order = 0; // source order across all sheets
};

struct StyleSheet {
    std::vector<Rule> rules;
};

// Parses a style sheet. Unsupported selectors drop the whole rule (W-CSS-UNSUPPORTED-SELECTOR); unsupported
// properties (W-CSS-UNSUPPORTED-PROPERTY) and invalid values (W-CSS-INVALID-VALUE) are ignored; `!important`
// is ignored (W-CSS-IMPORTANT-IGNORED); @import and url() never fetch anything (W-CSS-EXTERNAL-IGNORED).
StyleSheet parseStyleSheet(std::string_view css, core::DiagnosticsLog& log, std::size_t orderBase = 0);
StyleSheet parseStyleSheet(std::u32string_view css, core::DiagnosticsLog& log, std::size_t orderBase = 0);
// Parses the contents of a style="" attribute.
std::vector<Declaration> parseInlineStyle(std::string_view css, core::DiagnosticsLog& log);

// The 16 CSS 2.1 colour keywords, #rgb, #rrggbb, #rrggbbaa and rgb(r, g, b).
std::optional<core::Color> parseColor(std::string_view text);

} // namespace pxc::presentcss
