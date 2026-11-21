#pragma once
// Internal: simplified HTML tokenizer (data, tag open, end tag, attributes, comment, doctype, raw text for
// script/style), with entity decoding and line/column tracking.

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "pxc/core/diagnostic.hpp"

namespace pxc::html::detail {

struct Token {
    enum class Kind { StartTag, EndTag, Text, Comment, Doctype, Malformed, Eof };
    Kind kind = Kind::Eof;
    std::string name; // lower-case tag name
    std::vector<std::pair<std::string, std::u32string>> attrs;
    bool selfClosing = false;
    std::u32string text;
    int line = 1, column = 1;
};

std::vector<Token> tokenize(std::u32string_view src, std::vector<core::Diagnostic>& diags);
std::u32string decodeEntities(std::u32string_view s, std::vector<core::Diagnostic>& diags, int line, int column);

} // namespace pxc::html::detail
