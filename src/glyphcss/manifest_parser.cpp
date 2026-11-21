#include "pxc/glyphcss/font_manifest.hpp"

#include <map>
#include <set>

#include "decl_block.hpp"
#include "pxc/core/css_tokenizer.hpp"

namespace pxc::glyphcss {

using core::CssToken;
using core::CssTokenKind;
using core::Diagnostic;

core::Expected<FontManifest, std::vector<Diagnostic>> parseManifest(std::string_view cssText,
                                                                    const std::filesystem::path& source) {
    std::vector<Diagnostic> errors;
    std::vector<Diagnostic> tokDiags;
    const auto tokens = core::tokenizeCss(cssText, tokDiags, &source);
    for (auto& d : tokDiags) { d.code = "E-PACK-SYNTAX"; errors.push_back(d); }
    auto block = detail::readSingleRule(tokens, "font-pack", "E-PACK-SYNTAX", source, errors);
    if (!block) return core::Unexpected(std::move(errors));

    auto err = [&](const char* code, const std::string& msg, int line, int col) {
        errors.push_back(core::makeError(code, msg, &source, line, col));
    };

    FontManifest m;
    std::set<std::string, std::less<>> seen;

    const std::map<std::string, std::string FontManifest::*, std::less<>> stringProps = {
        {"--id", &FontManifest::id},
        {"--display-name", &FontManifest::displayName},
        {"--style-description", &FontManifest::styleDescription},
        {"--version", &FontManifest::version},
        {"--authenticity", &FontManifest::authenticity},
        {"--origin", &FontManifest::origin},
        {"--author", &FontManifest::author},
        {"--license", &FontManifest::license},
        {"--license-file", &FontManifest::licenseFile},
        {"--glyph-dir", &FontManifest::glyphDir},
    };
    const std::map<std::string, int FontManifest::*, std::less<>> intProps = {
        {"--line-height", &FontManifest::lineHeight},
        {"--ascent", &FontManifest::ascent},
        {"--descent", &FontManifest::descent},
        {"--order", &FontManifest::order},
    };

    for (const auto& d : block->decls) {
        if (!seen.insert(d.name).second) {
            err("E-PACK-SYNTAX", "property `" + d.name + "` is declared more than once", d.line, d.column);
            continue;
        }
        const CssToken* v0 = d.values.empty() ? nullptr : &d.values[0];
        const int vl = v0 ? v0->line : d.line, vc = v0 ? v0->column : d.column;
        if (d.name == "--schema") {
            if (!v0 || v0->kind != CssTokenKind::String || v0->text != kPackSchema)
                err("E-PACK-SCHEMA", std::string("--schema must be \"") + kPackSchema + "\"", vl, vc);
            else
                m.schema = v0->text;
        } else if (auto s = stringProps.find(d.name); s != stringProps.end()) {
            if (!v0 || d.values.size() != 1 || v0->kind != CssTokenKind::String)
                err("E-PACK-SYNTAX", d.name + " must be a quoted string", vl, vc);
            else
                m.*(s->second) = v0->text;
        } else if (auto ip = intProps.find(d.name); ip != intProps.end()) {
            if (!v0 || d.values.size() != 1 || v0->kind != CssTokenKind::Number || !v0->isInteger)
                err("E-PACK-SYNTAX", d.name + " must be an integer", vl, vc);
            else
                m.*(ip->second) = static_cast<int>(v0->number);
        } else if (d.name == "--coverage") {
            bool expectRange = true;
            for (const auto& t : d.values) {
                if (expectRange && t.kind == CssTokenKind::UnicodeRange && t.rangeStart <= t.rangeEnd) {
                    m.coverage.push_back({t.rangeStart, t.rangeEnd});
                    expectRange = false;
                } else if (!expectRange && t.kind == CssTokenKind::Comma) {
                    expectRange = true;
                } else {
                    err("E-PACK-SYNTAX", "--coverage must be a comma-separated list of U+XXXX or U+XXXX-YYYY", t.line,
                        t.column);
                    break;
                }
            }
        } else if (d.name == "--replacement-glyph") {
            if (!v0 || v0->kind != CssTokenKind::UnicodeRange || v0->rangeStart != v0->rangeEnd)
                err("E-PACK-SYNTAX", "--replacement-glyph must be a single code point", vl, vc);
            else
                m.replacementGlyph = v0->rangeStart;
        } else if (d.name == "--aliases") {
            bool expectString = true;
            for (const auto& t : d.values) {
                if (expectString && t.kind == CssTokenKind::String) { m.aliases.push_back(t.text); expectString = false; }
                else if (!expectString && t.kind == CssTokenKind::Comma) expectString = true;
                else { err("E-PACK-SYNTAX", "--aliases must be a comma-separated list of strings", t.line, t.column); break; }
            }
        } else {
            m.warnings.push_back(core::makeWarning("W-PACK-UNKNOWN-PROP", "unknown property `" + d.name + "` ignored",
                                                   &source, d.line, d.column));
        }
    }

    for (const char* req : {"--schema", "--id", "--display-name", "--style-description", "--version",
                            "--authenticity", "--origin", "--author", "--license", "--license-file",
                            "--line-height", "--ascent", "--descent", "--coverage"}) {
        if (!seen.count(req))
            err(std::string_view(req) == "--schema" ? "E-PACK-SCHEMA" : "E-PACK-MISSING-PROP",
                std::string("missing required property ") + req, block->line, block->column);
    }
    for (const auto& [name, ptr] : stringProps) {
        if (seen.count(name) && (m.*ptr).empty() && name != "--glyph-dir")
            err("E-PACK-MISSING-PROP", name + " must not be empty", block->line, block->column);
    }
    if (seen.count("--line-height") && seen.count("--ascent") && seen.count("--descent")) {
        if (m.lineHeight <= 0 || m.ascent < 0 || m.descent < 0 || m.ascent + m.descent > m.lineHeight)
            err("E-PACK-METRICS", "metrics invalid: ascent (" + std::to_string(m.ascent) + ") + descent (" +
                                      std::to_string(m.descent) + ") must be <= line-height (" +
                                      std::to_string(m.lineHeight) + ") and all values non-negative",
                block->line, block->column);
    }
    if (seen.count("--authenticity") && !m.authenticity.empty()) {
        if (m.authenticity != "style-interpretation" && m.authenticity != "licensed-original")
            err("E-PACK-AUTHENTICITY", "--authenticity must be \"style-interpretation\" or \"licensed-original\"",
                block->line, block->column);
        else if (m.authenticity == "licensed-original" && m.license.size() < 3)
            err("E-PACK-AUTHENTICITY", "licensed-original packs must name the rights holder in --license", block->line,
                block->column);
    }
    if (!m.id.empty()) {
        for (char c : m.id)
            if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) {
                err("E-PACK-SYNTAX", "--id must match [a-z0-9-]+", block->line, block->column);
                break;
            }
    }

    if (!errors.empty()) return core::Unexpected(std::move(errors));
    return m;
}

} // namespace pxc::glyphcss
