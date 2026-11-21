#pragma once
// Shared CSS Syntax Level 3 subset tokenizer. The glyph, manifest and presentation-CSS
// parsers are separate modules built on top of this tokenizer (FR-041).

#include <string>
#include <string_view>
#include <vector>

#include "pxc/core/diagnostic.hpp"

namespace pxc::core {

enum class CssTokenKind {
    Ident,        // text = identifier (escapes decoded)
    AtKeyword,    // text = name without '@'
    String,       // text = decoded string contents
    Hash,         // text = name without '#'
    Number,       // number = value, text = source spelling
    Percentage,   // number = value
    Dimension,    // number = value, unit = unit identifier
    UnicodeRange, // rangeStart/rangeEnd
    Function,     // text = function name (the '(' is consumed)
    Delim,        // text = the single character
    Colon,
    Semicolon,
    Comma,
    LeftBrace,
    RightBrace,
    LeftParen,
    RightParen,
    LeftBracket,
    RightBracket,
    Whitespace,
    BadString, // unterminated string (diagnostic emitted)
    Eof,
};

struct CssToken {
    CssTokenKind kind = CssTokenKind::Eof;
    std::string text;
    std::string unit;
    double number = 0.0;
    bool isInteger = false;
    char32_t rangeStart = 0;
    char32_t rangeEnd = 0;
    int line = 1;   // 1-based
    int column = 1; // 1-based, in bytes
};

const char* tokenKindName(CssTokenKind k) noexcept;

// Tokenizes UTF-8 input. Comments are skipped. Diagnostics (E-CSS-SYNTAX) are appended to `diags`
// for unterminated strings and comments. The returned vector always ends with an Eof token.
std::vector<CssToken> tokenizeCss(std::string_view input, std::vector<Diagnostic>& diags,
                                  const std::filesystem::path* file = nullptr);

} // namespace pxc::core
