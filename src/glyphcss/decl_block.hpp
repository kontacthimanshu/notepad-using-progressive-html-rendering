#pragma once
// Internal: reads a single `<selector> { --prop: value; ... }` rule from CSS tokens.
// Shared by the glyph and manifest parsers (both pixelcss dialects). Not used by presentation CSS.

#include <optional>
#include <string>
#include <vector>

#include "pxc/core/css_tokenizer.hpp"
#include "pxc/core/diagnostic.hpp"

namespace pxc::glyphcss::detail {

struct Declaration {
    std::string name;
    int line = 0, column = 0;
    std::vector<core::CssToken> values; // whitespace removed
};

struct RuleBlock {
    std::string selector;
    int line = 1, column = 1;
    std::vector<Declaration> decls;
};

// Parses exactly one rule whose selector is the identifier `expectedSelector`.
// Errors are reported with `syntaxCode`; returns nullopt when the structure is unusable.
std::optional<RuleBlock> readSingleRule(const std::vector<core::CssToken>& tokens, std::string_view expectedSelector,
                                        const char* syntaxCode, const std::filesystem::path& source,
                                        std::vector<core::Diagnostic>& errors);

} // namespace pxc::glyphcss::detail
