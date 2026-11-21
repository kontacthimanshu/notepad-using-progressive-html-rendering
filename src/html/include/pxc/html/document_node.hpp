#pragma once
// Restricted HTML document model (data-model §7, contracts/html-subset.md). Logically separate from glyph
// bitmaps and paint surfaces (FR-036). Read-only preview only (FR-038).

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "pxc/core/diagnostic.hpp"
#include "pxc/core/expected.hpp"
#include "pxc/core/geometry.hpp"
#include "pxc/fonts/font_registry.hpp"
#include "pxc/layout/block_layout.hpp"
#include "pxc/presentcss/stylesheet.hpp"

namespace pxc::html {

enum class Tag { Html, Head, Body, Title, Div, P, Span, Br, Style, Unknown, Text };

struct ComputedStyle {
    core::Color color{0, 0, 0, 255};
    core::Color background{0, 0, 0, 0};
    std::string packId;
    bool fontExplicit = false;  // resolved from a font-family declaration (here or inherited)
    double marginTopPx = 0;     // at scale 1
    double marginBottomPx = 0;
    double lineHeightMultiplier = 0; // 0 = normal
    double lineHeightPx = 0;         // > 0 overrides the multiplier
    presentcss::DisplayValue display = presentcss::DisplayValue::Inline;
};

struct DocumentNode {
    Tag tag = Tag::Unknown;
    std::string rawTag;
    std::string id;
    std::vector<std::string> classes;
    std::string inlineStyle;
    std::u32string text; // Tag::Text only
    std::vector<std::unique_ptr<DocumentNode>> children;
    DocumentNode* parent = nullptr;
    ComputedStyle style;
    int line = 0, column = 0;
};

struct PreviewDocument {
    std::unique_ptr<DocumentNode> root;
    std::u32string title;
    std::vector<std::string> styleSheets; // <style> contents, document order
    std::vector<core::Diagnostic> diagnostics;
    bool hasErrors() const noexcept;
};

const char* tagName(Tag t) noexcept;

// Never fails for non-empty input: malformed documents are recovered and reported in `diagnostics`.
core::Expected<PreviewDocument, std::vector<core::Diagnostic>> parseHtml(std::u32string_view source);

// Indented tree dump used by the contract tests (e.g. `div#main.note style="color: red"` / `#text "Hello"`).
std::string dumpTree(const PreviewDocument& doc);

// Applies <style> sheets and style="" attributes (cascade by specificity then source order), inheritance and
// font-family alias mapping. Elements without a resolved font use `activePackId`.
void computeStyles(PreviewDocument& doc, const fonts::FontRegistry& registry, std::string_view activePackId,
                   core::DiagnosticsLog& log);

// Flattens the styled tree into block boxes of whitespace-collapsed styled runs.
std::vector<layout::StyledBlock> toStyledBlocks(const PreviewDocument& doc);

} // namespace pxc::html
