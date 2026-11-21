#include "pxc/glyphcss/glyph_parser.hpp"

#include <cctype>
#include <map>
#include <set>

#include "decl_block.hpp"
#include "pxc/core/css_tokenizer.hpp"
#include "pxc/core/unicode.hpp"

namespace pxc::glyphcss {

using core::CssToken;
using core::CssTokenKind;
using core::Diagnostic;

std::uint64_t contentHash(std::string_view text) noexcept {
    std::uint64_t h = 1469598103934665603ull;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c == '\r' && i + 1 < text.size() && text[i + 1] == '\n') continue; // CRLF -> LF
        h ^= static_cast<unsigned char>(c);
        h *= 1099511628211ull;
    }
    return h;
}

std::string cellToken(const Cell& c) {
    switch (c.kind) {
    case CellKind::Transparent: return ".";
    case CellKind::CurrentColor: return "@";
    case CellKind::BackgroundColor: return "~";
    case CellKind::Literal: return c.rgba.toHex();
    }
    return ".";
}

std::optional<char32_t> codePointFromFileName(const std::filesystem::path& file) {
    const std::string stem = file.stem().string();
    if (file.extension() != ".css") return std::nullopt;
    if (stem.size() < 6 || stem.size() > 8 || stem[0] != 'U' || stem[1] != '+') return std::nullopt;
    char32_t cp = 0;
    for (std::size_t k = 2; k < stem.size(); ++k) {
        const char c = stem[k];
        int v;
        if (c >= '0' && c <= '9') v = c - '0';
        else if (c >= 'A' && c <= 'F') v = c - 'A' + 10;
        else return std::nullopt;
        cp = cp * 16 + static_cast<char32_t>(v);
    }
    return cp;
}

namespace {

struct IntLimits { int lo, hi; };

const std::map<std::string, IntLimits, std::less<>> kIntProps = {
    {"--width", {0, 64}}, {"--height", {0, 64}}, {"--baseline", {0, 64}},
    {"--advance", {0, 128}}, {"--bearing-x", {-16, 16}},
};

bool isMetaName(std::string_view n) {
    if (n.size() <= 7 || n.substr(0, 7) != "--meta-") return false;
    for (char c : n.substr(7))
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) return false;
    return true;
}

std::string lower(std::string_view s) {
    std::string o(s);
    for (char& c : o) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return o;
}

std::string hexCp(char32_t cp) {
    static constexpr char d[] = "0123456789ABCDEF";
    std::string s;
    for (int shift = (cp > 0xFFFF ? 20 : 12); shift >= 0; shift -= 4) s += d[(cp >> shift) & 0xF];
    return "U+" + s;
}

} // namespace

core::Expected<GlyphDefinition, std::vector<Diagnostic>>
parseGlyph(std::string_view cssText, const std::filesystem::path& source, std::optional<std::string_view> expectedFamily) {
    std::vector<Diagnostic> errors;
    std::vector<Diagnostic> warnings;
    std::vector<Diagnostic> tokDiags;
    const auto tokens = core::tokenizeCss(cssText, tokDiags, &source);
    for (auto& d : tokDiags) { d.code = "E-GLYPH-SYNTAX"; errors.push_back(d); }

    auto block = detail::readSingleRule(tokens, "glyph", "E-GLYPH-SYNTAX", source, errors);
    if (!block) return core::Unexpected(std::move(errors));

    auto err = [&](const char* code, const std::string& msg, int line, int col) {
        errors.push_back(core::makeError(code, msg, &source, line, col));
    };

    GlyphDefinition g;
    g.sourcePath = source;
    g.contentHash = contentHash(cssText);

    std::set<std::string, std::less<>> seen;
    std::map<std::string, int, std::less<>> ints;
    bool haveCp = false, haveFamily = false, haveSchema = false, haveRows = false;
    std::vector<const CssToken*> rowTokens;
    int rowsLine = block->line, rowsCol = block->column;

    for (const auto& d : block->decls) {
        if (!seen.insert(d.name).second) {
            err("E-GLYPH-DUP-PROP", "property `" + d.name + "` is declared more than once", d.line, d.column);
            continue;
        }
        const CssToken* v0 = d.values.empty() ? nullptr : &d.values[0];
        const int vl = v0 ? v0->line : d.line, vc = v0 ? v0->column : d.column;

        if (d.name == "--schema") {
            haveSchema = true;
            if (!v0 || v0->kind != CssTokenKind::String || d.values.size() != 1 || v0->text != kGlyphSchema) {
                err("E-GLYPH-SCHEMA", std::string("--schema must be \"") + kGlyphSchema + "\"", vl, vc);
            } else {
                g.schema = v0->text;
            }
        } else if (d.name == "--code-point") {
            if (!v0 || d.values.size() != 1 || v0->kind != CssTokenKind::UnicodeRange || v0->rangeStart != v0->rangeEnd ||
                !core::isValidScalar(v0->rangeStart)) {
                err("E-GLYPH-BAD-DIM", "--code-point must be a single code point such as U+0041", vl, vc);
            } else {
                g.codePoint = v0->rangeStart;
                haveCp = true;
            }
        } else if (d.name == "--family" || d.name == "--style" || d.name == "--note") {
            if (!v0 || d.values.size() != 1 || v0->kind != CssTokenKind::String) {
                err("E-GLYPH-SYNTAX", d.name + " must be a quoted string", vl, vc);
            } else if (d.name == "--family") {
                g.family = v0->text;
                haveFamily = true;
            } else if (d.name == "--style") {
                g.style = v0->text;
            }
        } else if (auto it = kIntProps.find(d.name); it != kIntProps.end()) {
            if (!v0 || d.values.size() != 1 || v0->kind != CssTokenKind::Number || !v0->isInteger) {
                err("E-GLYPH-BAD-DIM", d.name + " must be an integer", vl, vc);
            } else {
                const int val = static_cast<int>(v0->number);
                if (val < it->second.lo || val > it->second.hi) {
                    err("E-GLYPH-BAD-DIM", d.name + " = " + std::to_string(val) + " is out of range " +
                                               std::to_string(it->second.lo) + ".." + std::to_string(it->second.hi),
                        vl, vc);
                } else {
                    ints[d.name] = val;
                }
            }
        } else if (d.name == "--rows") {
            haveRows = true;
            rowsLine = d.line;
            rowsCol = d.column;
            for (const auto& t : d.values) {
                if (t.kind != CssTokenKind::String) {
                    if (t.kind != CssTokenKind::BadString)
                        err("E-GLYPH-SYNTAX", "--rows must be a sequence of quoted strings", t.line, t.column);
                    continue;
                }
                rowTokens.push_back(&t);
            }
        } else if (isMetaName(d.name)) {
            if (!v0 || d.values.size() != 1 ||
                (v0->kind != CssTokenKind::String && v0->kind != CssTokenKind::Number)) {
                err("E-GLYPH-SYNTAX", d.name + " must be a string or number", vl, vc);
            } else {
                g.metadata.emplace_back(d.name.substr(7), v0->text);
            }
        } else {
            warnings.push_back(core::makeWarning("W-GLYPH-UNKNOWN-PROP", "unknown property `" + d.name + "` ignored",
                                                 &source, d.line, d.column));
        }
    }

    auto requireInt = [&](const char* name) {
        if (!seen.count(name))
            err("E-GLYPH-MISSING-PROP", std::string("missing required property ") + name, block->line, block->column);
    };
    if (!haveSchema)
        err("E-GLYPH-MISSING-PROP", "missing required property --schema", block->line, block->column);
    if (!seen.count("--code-point"))
        err("E-GLYPH-MISSING-PROP", "missing required property --code-point", block->line, block->column);
    if (!seen.count("--family"))
        err("E-GLYPH-MISSING-PROP", "missing required property --family", block->line, block->column);
    for (const char* n : {"--width", "--height", "--baseline", "--advance"}) requireInt(n);

    g.width = ints.count("--width") ? ints["--width"] : 0;
    g.height = ints.count("--height") ? ints["--height"] : 0;
    g.baseline = ints.count("--baseline") ? ints["--baseline"] : 0;
    g.advance = ints.count("--advance") ? ints["--advance"] : 0;
    g.bearingX = ints.count("--bearing-x") ? ints["--bearing-x"] : 0;

    const bool dimsKnown = ints.count("--width") && ints.count("--height");
    // --baseline may exceed --height: such glyphs float above the baseline (e.g. '-', '^', quotes).
    if (dimsKnown && g.height > 0 && !haveRows) {
        err("E-GLYPH-MISSING-PROP", "missing required property --rows", block->line, block->column);
    }

    if (dimsKnown && haveRows) {
        if (static_cast<int>(rowTokens.size()) != g.height) {
            err("E-GLYPH-ROW-COUNT", "--rows has " + std::to_string(rowTokens.size()) + " row(s), expected " +
                                         std::to_string(g.height),
                rowsLine, rowsCol);
        } else {
            g.cells.reserve(static_cast<std::size_t>(g.width) * static_cast<std::size_t>(g.height));
            for (std::size_t r = 0; r < rowTokens.size(); ++r) {
                const CssToken& rt = *rowTokens[r];
                const std::string& s = rt.text;
                std::vector<Cell> rowCells;
                bool rowOk = true;
                std::size_t p = 0;
                while (p < s.size()) {
                    while (p < s.size() && s[p] == ' ') ++p;
                    if (p >= s.size()) break;
                    const std::size_t start = p;
                    while (p < s.size() && s[p] != ' ') ++p;
                    const std::string tok = lower(std::string_view(s).substr(start, p - start));
                    const int col = rt.column + 1 + static_cast<int>(start);
                    Cell c;
                    if (tok == "." || tok == "transparent") {
                        c.kind = CellKind::Transparent;
                    } else if (tok == "@" || tok == "currentcolor") {
                        c.kind = CellKind::CurrentColor;
                    } else if (tok == "~" || tok == "backgroundcolor") {
                        c.kind = CellKind::BackgroundColor;
                    } else if (auto col8 = core::Color::parseHex(tok)) {
                        if (col8->a == 0) c.kind = CellKind::Transparent;
                        else { c.kind = CellKind::Literal; c.rgba = *col8; }
                    } else {
                        err("E-GLYPH-BAD-COLOR", "invalid cell token `" + s.substr(start, p - start) + "`", rt.line, col);
                        rowOk = false;
                    }
                    rowCells.push_back(c);
                }
                if (static_cast<int>(rowCells.size()) != g.width) {
                    err("E-GLYPH-ROW-LENGTH", "row " + std::to_string(r + 1) + " has " + std::to_string(rowCells.size()) +
                                                  " cells, expected " + std::to_string(g.width),
                        rt.line, rt.column);
                    rowOk = false;
                }
                if (rowOk) g.cells.insert(g.cells.end(), rowCells.begin(), rowCells.end());
            }
        }
    }

    if (haveCp) {
        if (auto fileCp = codePointFromFileName(source); fileCp && *fileCp != g.codePoint) {
            err("E-GLYPH-CP-MISMATCH", "--code-point " + hexCp(g.codePoint) + " does not match file name " +
                                           source.filename().string(),
                block->line, block->column);
        }
    }
    if (haveFamily && expectedFamily && g.family != *expectedFamily) {
        err("E-GLYPH-FAMILY-MISMATCH", "--family \"" + g.family + "\" does not match pack id \"" +
                                           std::string(*expectedFamily) + "\"",
            block->line, block->column);
    }

    if (!errors.empty()) return core::Unexpected(std::move(errors));
    g.warnings = std::move(warnings);
    return g;
}

} // namespace pxc::glyphcss
