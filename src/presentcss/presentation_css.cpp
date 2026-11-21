#include "pxc/presentcss/stylesheet.hpp"

#include <array>
#include <cctype>
#include <charconv>

#include "pxc/core/css_tokenizer.hpp"
#include "pxc/core/unicode.hpp"

namespace pxc::presentcss {

using core::CssToken;
using core::CssTokenKind;

namespace {

std::string lower(std::string_view s) {
    std::string o(s);
    for (char& c : o) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return o;
}

void warn(core::DiagnosticsLog& log, const char* code, const std::string& msg, const CssToken& at) {
    log.add(core::makeWarning(code, msg + " (line " + std::to_string(at.line) + ", column " + std::to_string(at.column) + ")"));
}

struct NamedColor {
    const char* name;
    core::Color c;
};
constexpr std::array<NamedColor, 16> kNamed = {{
    {"black", {0, 0, 0, 255}},       {"silver", {192, 192, 192, 255}}, {"gray", {128, 128, 128, 255}},
    {"white", {255, 255, 255, 255}}, {"maroon", {128, 0, 0, 255}},     {"red", {255, 0, 0, 255}},
    {"purple", {128, 0, 128, 255}},  {"fuchsia", {255, 0, 255, 255}},  {"green", {0, 128, 0, 255}},
    {"lime", {0, 255, 0, 255}},      {"olive", {128, 128, 0, 255}},    {"yellow", {255, 255, 0, 255}},
    {"navy", {0, 0, 128, 255}},      {"blue", {0, 0, 255, 255}},       {"teal", {0, 128, 128, 255}},
    {"aqua", {0, 255, 255, 255}},
}};

std::vector<CssToken> significant(const std::vector<CssToken>& in, std::size_t from, std::size_t to) {
    std::vector<CssToken> out;
    for (std::size_t i = from; i < to; ++i)
        if (in[i].kind != CssTokenKind::Whitespace) out.push_back(in[i]);
    return out;
}

std::optional<core::Color> colorFromTokens(const std::vector<CssToken>& v) {
    if (v.empty()) return std::nullopt;
    const CssToken& t = v[0];
    if (v.size() == 1 && t.kind == CssTokenKind::Ident) return parseColor(t.text);
    if (v.size() == 1 && t.kind == CssTokenKind::Hash) return parseColor("#" + t.text);
    if (t.kind == CssTokenKind::Function && lower(t.text) == "rgb") {
        std::vector<int> nums;
        for (std::size_t i = 1; i < v.size(); ++i) {
            if (v[i].kind == CssTokenKind::Number && v[i].isInteger && v[i].number >= 0 && v[i].number <= 255)
                nums.push_back(static_cast<int>(v[i].number));
            else if (v[i].kind == CssTokenKind::Comma) continue;
            else if (v[i].kind == CssTokenKind::RightParen) break;
            else return std::nullopt;
        }
        if (nums.size() != 3) return std::nullopt;
        return core::Color{static_cast<std::uint8_t>(nums[0]), static_cast<std::uint8_t>(nums[1]),
                           static_cast<std::uint8_t>(nums[2]), 255};
    }
    return std::nullopt;
}

bool hasUrl(const std::vector<CssToken>& v) {
    for (const auto& t : v)
        if ((t.kind == CssTokenKind::Function && lower(t.text) == "url") ||
            (t.kind == CssTokenKind::Ident && lower(t.text).rfind("url", 0) == 0 && t.text.size() == 3))
            return true;
    return false;
}

// Parses one declaration's value tokens. Returns nullopt (with a diagnostic) when ignored.
std::optional<Declaration> parseDeclaration(const std::string& name, std::vector<CssToken> v, const CssToken& at,
                                            core::DiagnosticsLog& log) {
    // !important is recognised and ignored.
    for (std::size_t i = 0; i + 1 < v.size(); ++i) {
        if (v[i].kind == CssTokenKind::Delim && v[i].text == "!" && v[i + 1].kind == CssTokenKind::Ident &&
            lower(v[i + 1].text) == "important") {
            warn(log, "W-CSS-IMPORTANT-IGNORED", "!important is ignored", v[i]);
            v.erase(v.begin() + static_cast<std::ptrdiff_t>(i), v.begin() + static_cast<std::ptrdiff_t>(i + 2));
            break;
        }
    }
    if (hasUrl(v)) {
        warn(log, "W-CSS-EXTERNAL-IGNORED", "url() resources are never fetched; declaration ignored", at);
        return std::nullopt;
    }
    const std::string prop = lower(name);
    Declaration d;
    auto invalid = [&]() -> std::optional<Declaration> {
        warn(log, "W-CSS-INVALID-VALUE", "invalid value for '" + prop + "'", at);
        return std::nullopt;
    };
    if (prop == "color" || prop == "background-color") {
        d.property = prop == "color" ? Property::Color : Property::BackgroundColor;
        d.color = colorFromTokens(v);
        if (!d.color) return invalid();
        if (d.property == Property::Color && d.color->a == 0) return invalid(); // 'transparent' text is not supported
        return d;
    }
    if (prop == "font-family") {
        d.property = Property::FontFamily;
        std::string cur;
        for (const auto& t : v) {
            if (t.kind == CssTokenKind::String) {
                if (!cur.empty()) return invalid();
                cur = t.text;
            } else if (t.kind == CssTokenKind::Ident) {
                if (!cur.empty()) cur += ' ';
                cur += t.text;
            } else if (t.kind == CssTokenKind::Comma) {
                if (cur.empty()) return invalid();
                d.families.push_back(cur);
                cur.clear();
            } else {
                return invalid();
            }
        }
        if (!cur.empty()) d.families.push_back(cur);
        if (d.families.empty()) return invalid();
        return d;
    }
    if (prop == "margin-top" || prop == "margin-bottom") {
        d.property = prop == "margin-top" ? Property::MarginTop : Property::MarginBottom;
        if (v.size() != 1) return invalid();
        const auto& t = v[0];
        if (t.kind == CssTokenKind::Number && t.number == 0) d.length = Length{0, Unit::Px};
        else if (t.kind == CssTokenKind::Dimension && lower(t.unit) == "px" && t.number >= 0) d.length = Length{t.number, Unit::Px};
        else if (t.kind == CssTokenKind::Dimension && lower(t.unit) == "em" && t.number >= 0) d.length = Length{t.number, Unit::Em};
        else return invalid();
        return d;
    }
    if (prop == "line-height") {
        d.property = Property::LineHeight;
        if (v.size() != 1) return invalid();
        const auto& t = v[0];
        if (t.kind == CssTokenKind::Ident && lower(t.text) == "normal") d.lineHeightNormal = true;
        else if (t.kind == CssTokenKind::Number && t.number > 0) d.length = Length{t.number, Unit::None};
        else if (t.kind == CssTokenKind::Dimension && lower(t.unit) == "px" && t.number > 0) d.length = Length{t.number, Unit::Px};
        else return invalid();
        return d;
    }
    if (prop == "display") {
        d.property = Property::Display;
        if (v.size() != 1 || v[0].kind != CssTokenKind::Ident) return invalid();
        const std::string k = lower(v[0].text);
        if (k == "block") d.display = DisplayValue::Block;
        else if (k == "inline") d.display = DisplayValue::Inline;
        else if (k == "none") d.display = DisplayValue::None;
        else return invalid();
        return d;
    }
    warn(log, "W-CSS-UNSUPPORTED-PROPERTY", "unsupported property '" + prop + "' ignored", at);
    return std::nullopt;
}

// Parses declarations between [from, to) (no braces).
std::vector<Declaration> parseDeclarations(const std::vector<CssToken>& t, std::size_t from, std::size_t to,
                                           core::DiagnosticsLog& log) {
    std::vector<Declaration> out;
    std::size_t i = from;
    while (i < to) {
        while (i < to && (t[i].kind == CssTokenKind::Whitespace || t[i].kind == CssTokenKind::Semicolon)) ++i;
        if (i >= to) break;
        std::size_t end = i;
        int depth = 0;
        while (end < to && !(depth == 0 && t[end].kind == CssTokenKind::Semicolon)) {
            if (t[end].kind == CssTokenKind::Function || t[end].kind == CssTokenKind::LeftParen) ++depth;
            if (t[end].kind == CssTokenKind::RightParen && depth > 0) --depth;
            ++end;
        }
        const auto decl = significant(t, i, end);
        if (decl.size() >= 2 && decl[0].kind == CssTokenKind::Ident && decl[1].kind == CssTokenKind::Colon) {
            std::vector<CssToken> value(decl.begin() + 2, decl.end());
            if (auto d = parseDeclaration(decl[0].text, std::move(value), decl[0], log)) out.push_back(std::move(*d));
        } else if (!decl.empty()) {
            warn(log, "W-CSS-INVALID-VALUE", "malformed declaration ignored", decl[0]);
        }
        i = end;
    }
    return out;
}

// Parses a selector list; returns false when any selector is unsupported.
bool parseSelectors(const std::vector<CssToken>& t, std::size_t from, std::size_t to, std::vector<Selector>& out) {
    Selector cur;
    bool any = false;
    bool sawWhitespaceAfter = false;
    for (std::size_t i = from; i < to; ++i) {
        const CssToken& k = t[i];
        if (k.kind == CssTokenKind::Whitespace) {
            if (any) sawWhitespaceAfter = true;
            continue;
        }
        if (k.kind == CssTokenKind::Comma) {
            if (!any) return false;
            out.push_back(cur);
            cur = {};
            any = false;
            sawWhitespaceAfter = false;
            continue;
        }
        if (sawWhitespaceAfter) return false; // descendant combinator
        if (k.kind == CssTokenKind::Ident && !any) {
            cur.tag = lower(k.text);
            any = true;
        } else if (k.kind == CssTokenKind::Delim && k.text == "." && i + 1 < to && t[i + 1].kind == CssTokenKind::Ident &&
                   cur.cls.empty()) {
            cur.cls = t[i + 1].text;
            ++i;
            any = true;
        } else if (k.kind == CssTokenKind::Hash && cur.id.empty()) {
            cur.id = k.text;
            any = true;
        } else {
            return false; // pseudo-classes, attribute selectors, '>', '+', '*', ...
        }
    }
    if (!any) return false;
    out.push_back(cur);
    return true;
}

} // namespace

bool Selector::matches(std::string_view t, std::string_view i, const std::vector<std::string>& classes) const {
    if (!tag.empty() && tag != t) return false;
    if (!id.empty() && id != i) return false;
    if (!cls.empty()) {
        bool found = false;
        for (const auto& c : classes) found |= c == cls;
        if (!found) return false;
    }
    return true;
}

std::optional<core::Color> parseColor(std::string_view text) {
    const std::string s = lower(text);
    if (s == "transparent") return core::Color{0, 0, 0, 0};
    for (const auto& n : kNamed)
        if (s == n.name) return n.c;
    if (!s.empty() && s[0] == '#') {
        if (s.size() == 4) {
            const std::string full = std::string("#") + s[1] + s[1] + s[2] + s[2] + s[3] + s[3];
            return core::Color::parseHex(full);
        }
        return core::Color::parseHex(s);
    }
    if (s.rfind("rgb(", 0) == 0) {
        std::vector<core::Diagnostic> d;
        auto toks = core::tokenizeCss(text, d);
        return colorFromTokens(significant(toks, 0, toks.size() - 1));
    }
    return std::nullopt;
}

StyleSheet parseStyleSheet(std::string_view css, core::DiagnosticsLog& log, std::size_t orderBase) {
    StyleSheet sheet;
    std::vector<core::Diagnostic> tokDiags;
    const auto t = core::tokenizeCss(css, tokDiags);
    for (auto& d : tokDiags) {
        d.code = "W-CSS-SYNTAX";
        d.severity = core::Severity::Warning;
        log.add(d);
    }
    std::size_t i = 0, order = orderBase;
    while (i < t.size() && t[i].kind != CssTokenKind::Eof) {
        if (t[i].kind == CssTokenKind::Whitespace || t[i].kind == CssTokenKind::Semicolon) { ++i; continue; }
        if (t[i].kind == CssTokenKind::AtKeyword) {
            warn(log, "W-CSS-EXTERNAL-IGNORED", "@" + t[i].text + " is not supported and nothing is fetched", t[i]);
            // Skip to ';' or a whole {...} block.
            while (i < t.size() && t[i].kind != CssTokenKind::Semicolon && t[i].kind != CssTokenKind::LeftBrace &&
                   t[i].kind != CssTokenKind::Eof)
                ++i;
            if (i < t.size() && t[i].kind == CssTokenKind::LeftBrace) {
                int depth = 0;
                for (; i < t.size() && t[i].kind != CssTokenKind::Eof; ++i) {
                    if (t[i].kind == CssTokenKind::LeftBrace) ++depth;
                    if (t[i].kind == CssTokenKind::RightBrace && --depth == 0) { ++i; break; }
                }
            }
            continue;
        }
        const std::size_t selStart = i;
        while (i < t.size() && t[i].kind != CssTokenKind::LeftBrace && t[i].kind != CssTokenKind::Eof) ++i;
        if (i >= t.size() || t[i].kind != CssTokenKind::LeftBrace) break;
        const std::size_t selEnd = i++;
        const std::size_t bodyStart = i;
        while (i < t.size() && t[i].kind != CssTokenKind::RightBrace && t[i].kind != CssTokenKind::Eof) ++i;
        const std::size_t bodyEnd = i;
        if (i < t.size() && t[i].kind == CssTokenKind::RightBrace) ++i;
        Rule rule;
        rule.order = order++;
        if (!parseSelectors(t, selStart, selEnd, rule.selectors)) {
            warn(log, "W-CSS-UNSUPPORTED-SELECTOR", "unsupported selector; rule ignored", t[selStart]);
            continue;
        }
        rule.declarations = parseDeclarations(t, bodyStart, bodyEnd, log);
        sheet.rules.push_back(std::move(rule));
    }
    return sheet;
}

StyleSheet parseStyleSheet(std::u32string_view css, core::DiagnosticsLog& log, std::size_t orderBase) {
    return parseStyleSheet(core::encodeUtf8(css), log, orderBase);
}

std::vector<Declaration> parseInlineStyle(std::string_view css, core::DiagnosticsLog& log) {
    std::vector<core::Diagnostic> tokDiags;
    const auto t = core::tokenizeCss(css, tokDiags);
    return parseDeclarations(t, 0, t.size() - 1, log);
}

} // namespace pxc::presentcss
