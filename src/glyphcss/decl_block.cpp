#include "decl_block.hpp"

namespace pxc::glyphcss::detail {

using core::CssToken;
using core::CssTokenKind;

std::optional<RuleBlock> readSingleRule(const std::vector<CssToken>& tokens, std::string_view expectedSelector,
                                        const char* syntaxCode, const std::filesystem::path& source,
                                        std::vector<core::Diagnostic>& errors) {
    std::size_t i = 0;
    auto skipWs = [&] {
        while (i < tokens.size() && tokens[i].kind == CssTokenKind::Whitespace) ++i;
    };
    auto err = [&](const std::string& msg, const CssToken& t) {
        errors.push_back(core::makeError(syntaxCode, msg, &source, t.line, t.column));
    };

    skipWs();
    const CssToken& sel = tokens[i];
    if (sel.kind != CssTokenKind::Ident || sel.text != expectedSelector) {
        err("expected `" + std::string(expectedSelector) + " {`", sel);
        return std::nullopt;
    }
    RuleBlock block;
    block.selector = sel.text;
    block.line = sel.line;
    block.column = sel.column;
    ++i;
    skipWs();
    if (tokens[i].kind != CssTokenKind::LeftBrace) {
        err("expected `{` after `" + std::string(expectedSelector) + "`", tokens[i]);
        return std::nullopt;
    }
    ++i;

    bool closed = false;
    while (i < tokens.size()) {
        skipWs();
        const CssToken& t = tokens[i];
        if (t.kind == CssTokenKind::Eof) break;
        if (t.kind == CssTokenKind::RightBrace) { closed = true; ++i; break; }
        if (t.kind == CssTokenKind::Semicolon) { ++i; continue; }
        if (t.kind != CssTokenKind::Ident) {
            err(std::string("expected a property name, found ") + core::tokenKindName(t.kind), t);
            // Skip to the next semicolon or closing brace.
            while (i < tokens.size() && tokens[i].kind != CssTokenKind::Semicolon &&
                   tokens[i].kind != CssTokenKind::RightBrace && tokens[i].kind != CssTokenKind::Eof)
                ++i;
            continue;
        }
        Declaration d;
        d.name = t.text;
        d.line = t.line;
        d.column = t.column;
        ++i;
        skipWs();
        if (tokens[i].kind != CssTokenKind::Colon) {
            err("expected `:` after property `" + d.name + "`", tokens[i]);
            while (i < tokens.size() && tokens[i].kind != CssTokenKind::Semicolon &&
                   tokens[i].kind != CssTokenKind::RightBrace && tokens[i].kind != CssTokenKind::Eof)
                ++i;
            continue;
        }
        ++i;
        while (i < tokens.size()) {
            const CssToken& v = tokens[i];
            if (v.kind == CssTokenKind::Semicolon || v.kind == CssTokenKind::RightBrace ||
                v.kind == CssTokenKind::Eof)
                break;
            if (v.kind != CssTokenKind::Whitespace) d.values.push_back(v);
            ++i;
        }
        block.decls.push_back(std::move(d));
    }
    if (!closed) {
        err("missing `}` at end of `" + std::string(expectedSelector) + "` rule", tokens[std::min(i, tokens.size() - 1)]);
        return std::nullopt;
    }
    skipWs();
    if (i < tokens.size() && tokens[i].kind != CssTokenKind::Eof) {
        err("unexpected content after the `" + std::string(expectedSelector) + "` rule (one rule per file)", tokens[i]);
        return std::nullopt;
    }
    return block;
}

} // namespace pxc::glyphcss::detail
