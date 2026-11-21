// Font pack loading and validation (contracts/font-manifest-schema.md).
#include "pack_internal.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <set>

#include "pxc/glyphcss/glyph_parser.hpp"

namespace pxc::fonts {

namespace fs = std::filesystem;
using core::Diagnostic;

namespace {

std::optional<std::string> readFile(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return std::nullopt;
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

bool isCanonicalGlyphName(const std::string& name) {
    // ^U\+[0-9A-F]{4,6}\.css$
    if (name.size() < 10 || name.size() > 12) return false;
    if (name[0] != 'U' || name[1] != '+' || name.substr(name.size() - 4) != ".css") return false;
    for (std::size_t i = 2; i < name.size() - 4; ++i) {
        const char c = name[i];
        if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'))) return false;
    }
    return true;
}

std::optional<char32_t> looseCodePoint(const std::string& name) {
    // U+<1..6 hex, any case>.css — used to detect duplicate identities under non-canonical names.
    if (name.size() < 7 || (name[0] != 'U' && name[0] != 'u') || name[1] != '+') return std::nullopt;
    if (name.size() < 4 || name.substr(name.size() - 4) != ".css") return std::nullopt;
    const std::string hex = name.substr(2, name.size() - 6);
    if (hex.empty() || hex.size() > 6) return std::nullopt;
    char32_t cp = 0;
    for (char c : hex) {
        int v;
        if (c >= '0' && c <= '9') v = c - '0';
        else if (c >= 'a' && c <= 'f') v = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') v = c - 'A' + 10;
        else return std::nullopt;
        cp = cp * 16 + static_cast<char32_t>(v);
    }
    return cp;
}

std::string cpName(char32_t cp) {
    static constexpr char d[] = "0123456789ABCDEF";
    std::string s;
    for (int shift = (cp > 0xFFFF ? 20 : 12); shift >= 0; shift -= 4) s += d[(cp >> shift) & 0xF];
    return "U+" + s;
}

void checkGlyphAgainstMetrics(const glyphcss::GlyphDefinition& g, const glyphcss::FontManifest& m,
                              std::vector<Diagnostic>& out) {
    const fs::path& p = g.sourcePath;
    if (g.baseline > m.ascent || g.height - g.baseline > m.descent) {
        out.push_back(core::makeWarning(
            "W-GLYPH-EXCEEDS-METRICS",
            cpName(g.codePoint) + " extends beyond the pack metrics (baseline " + std::to_string(g.baseline) +
                " vs ascent " + std::to_string(m.ascent) + ", descender rows " + std::to_string(g.height - g.baseline) +
                " vs descent " + std::to_string(m.descent) + ")",
            &p, 1, 1));
    }
    if (g.width > 0 && g.height > 0 && g.cells.size() == static_cast<std::size_t>(g.width * g.height)) {
        auto rowEmpty = [&](int r) {
            for (int c = 0; c < g.width; ++c)
                if (g.cellAt(c, r).kind != glyphcss::CellKind::Transparent) return false;
            return true;
        };
        auto colEmpty = [&](int c) {
            for (int r = 0; r < g.height; ++r)
                if (g.cellAt(c, r).kind != glyphcss::CellKind::Transparent) return false;
            return true;
        };
        if (rowEmpty(0) || rowEmpty(g.height - 1) || colEmpty(0) || colEmpty(g.width - 1)) {
            out.push_back(core::makeWarning("W-GLYPH-UNTRIMMED",
                                            cpName(g.codePoint) + " has fully transparent edge rows or columns",
                                            &p, 1, 1));
        }
    }
}

} // namespace

const char* packStateName(PackState s) noexcept {
    switch (s) {
    case PackState::Loading: return "Loading";
    case PackState::Valid: return "Valid";
    case PackState::Degraded: return "Degraded";
    case PackState::Invalid: return "Invalid";
    }
    return "?";
}

std::unique_ptr<FontRegistry::Pack> loadPackFromDisk(const fs::path& dir) {
    auto pack = std::make_unique<FontRegistry::Pack>();
    pack->dir = dir;
    pack->state = PackState::Loading;
    auto& diags = pack->diagnostics;

    const fs::path manifestPath = dir / "manifest.css";
    auto text = readFile(manifestPath);
    if (!text) {
        diags.push_back(core::makeError("E-PACK-MANIFEST", "manifest.css is missing or unreadable", &manifestPath, 0, 0));
        pack->state = PackState::Invalid;
        pack->id = dir.filename().string();
        return pack;
    }
    auto man = glyphcss::parseManifest(*text, manifestPath);
    if (!man) {
        diags.insert(diags.end(), man.error().begin(), man.error().end());
        pack->state = PackState::Invalid;
        pack->id = dir.filename().string();
        return pack;
    }
    pack->manifest = std::move(*man);
    diags.insert(diags.end(), pack->manifest.warnings.begin(), pack->manifest.warnings.end());
    pack->id = pack->manifest.id;
    const auto& m = pack->manifest;

    bool invalid = false;
    if (m.id != dir.filename().string()) {
        diags.push_back(core::makeError("E-PACK-ID-MISMATCH",
                                        "--id \"" + m.id + "\" does not match directory name \"" +
                                            dir.filename().string() + "\"",
                                        &manifestPath, 1, 1));
        invalid = true;
    }
    {
        std::error_code ec;
        if (!fs::is_regular_file(dir / m.licenseFile, ec)) {
            diags.push_back(core::makeError("E-PACK-LICENSE-FILE", "license file \"" + m.licenseFile + "\" not found",
                                            &manifestPath, 1, 1));
            invalid = true;
        }
    }
    if (invalid) {
        pack->state = PackState::Invalid;
        return pack;
    }

    // Enumerate glyph files.
    bool degraded = false;
    const fs::path glyphDir = dir / m.glyphDir;
    std::map<char32_t, fs::path> canonical;
    std::vector<std::pair<char32_t, fs::path>> nonCanonical;
    std::error_code ec;
    if (fs::is_directory(glyphDir, ec)) {
        std::vector<fs::path> files;
        for (const auto& e : fs::directory_iterator(glyphDir, ec))
            if (e.is_regular_file(ec)) files.push_back(e.path());
        std::sort(files.begin(), files.end());
        for (const auto& f : files) {
            const std::string name = f.filename().string();
            if (isCanonicalGlyphName(name)) {
                canonical[*glyphcss::codePointFromFileName(f)] = f;
            } else if (auto cp = looseCodePoint(name)) {
                nonCanonical.emplace_back(*cp, f);
            } else {
                diags.push_back(core::makeWarning("W-PACK-IGNORED-FILE", "ignored non-glyph file " + name, &f, 0, 0));
            }
        }
    }
    for (const auto& [cp, f] : nonCanonical) {
        if (canonical.count(cp)) {
            diags.push_back(core::makeError("E-PACK-DUP-GLYPH",
                                            "more than one file maps to " + cpName(cp) + ": " +
                                                canonical[cp].filename().string() + " and " + f.filename().string(),
                                            &f, 0, 0));
            degraded = true;
        } else {
            diags.push_back(core::makeWarning("W-PACK-IGNORED-FILE",
                                              "glyph file name must be U+XXXX.css with 4-6 uppercase hex digits: " +
                                                  f.filename().string(),
                                              &f, 0, 0));
        }
    }

    for (const auto& [cp, f] : canonical) {
        auto gtext = readFile(f);
        if (!gtext) {
            diags.push_back(core::makeError("E-GLYPH-UNREADABLE", "cannot read glyph file", &f, 0, 0));
            degraded = true;
            continue;
        }
        auto g = glyphcss::parseGlyph(*gtext, f, m.id);
        if (!g) {
            diags.insert(diags.end(), g.error().begin(), g.error().end());
            degraded = true;
            continue;
        }
        diags.insert(diags.end(), g->warnings.begin(), g->warnings.end());
        checkGlyphAgainstMetrics(*g, m, diags);
        if (!m.covers(cp) && cp != m.replacementGlyph) {
            diags.push_back(core::makeWarning("W-PACK-EXTRA-GLYPH", cpName(cp) + " is outside the declared coverage",
                                              &f, 0, 0));
        }
        pack->glyphs.emplace(cp, std::move(*g));
    }

    std::size_t missing = 0;
    for (const auto& r : m.coverage) {
        for (char32_t cp = r.first; cp <= r.last; ++cp) {
            if (!pack->glyphs.count(cp)) {
                ++missing;
                const fs::path expected = glyphDir / (cpName(cp) + ".css");
                diags.push_back(core::makeError("E-PACK-COVERAGE-MISSING",
                                                cpName(cp) + " is covered by the manifest but has no valid glyph file",
                                                &expected, 0, 0));
            }
            if (cp == r.last) break; // avoid overflow at U+10FFFF
        }
    }
    pack->coverageMissing = missing;
    if (missing > 0) degraded = true;
    pack->state = degraded ? PackState::Degraded : PackState::Valid;
    return pack;
}

} // namespace pxc::fonts
