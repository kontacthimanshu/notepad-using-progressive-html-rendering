#include <cstdio>
#include <filesystem>

#include "pxc/fonts/font_registry.hpp"
#include "tool.hpp"

namespace pxc::tool {

int cmdValidate(const Args& a) {
    namespace fs = std::filesystem;
    if (a.positional.size() != 1) {
        std::fputs("validate: expected <fonts-dir|pack-dir>\n", stderr);
        return 2;
    }
    const fs::path target = a.positional[0];
    const bool strict = a.has("strict");
    core::DiagnosticsLog log;
    fonts::FontRegistry reg(log);
    std::error_code ec;
    if (fs::exists(target / "manifest.css", ec)) reg.loadPack(target);
    else if (fs::is_directory(target, ec)) reg.loadAll(target);
    else {
        std::fprintf(stderr, "validate: %s is not a directory\n", target.generic_string().c_str());
        return 2;
    }

    std::size_t errors = 0, warnings = 0;
    for (const auto& d : log.all()) {
        std::printf("%s\n", core::format(d).c_str());
        if (d.severity == core::Severity::Error) ++errors;
        if (d.severity == core::Severity::Warning) ++warnings;
    }

    const auto packs = reg.packs();
    std::size_t totalAscii = 0;
    std::printf("\n%-22s %-9s %-17s %-11s %-7s %s\n", "id", "state", "covered/declared", "ASCII", "U+FFFD", "provenance");
    for (const auto& p : packs) {
        std::size_t ascii = 0;
        bool hasFffd = false;
        for (const auto* g : reg.glyphs(p.id)) {
            if (g->codePoint >= 0x20 && g->codePoint <= 0x7E) ++ascii;
            if (g->codePoint == 0xFFFD) hasFffd = true;
        }
        totalAscii += ascii;
        const std::size_t covered = p.coverageDeclared - p.coverageMissing;
        const bool provenance = !p.origin.empty() && !p.author.empty() && !p.version.empty() && !p.license.empty() &&
                                !p.authenticity.empty();
        std::printf("%-22s %-9s %7zu/%-9zu %3zu/95%-5s %-7s %s\n", p.id.c_str(), fonts::packStateName(p.state), covered,
                    p.coverageDeclared, ascii, "", hasFffd ? "yes" : "no",
                    provenance ? (p.authenticity + ", " + p.license + ", " + p.author).c_str() : "MISSING");
        if (strict && p.state == fonts::PackState::Valid && (ascii != 95 || !hasFffd || !provenance)) {
            std::printf("  strict: %s requires 95/95 ASCII, U+FFFD and complete provenance\n", p.id.c_str());
            ++errors;
        }
        if (p.state != fonts::PackState::Valid && strict) ++errors;
    }
    std::printf("\ntotal ASCII glyph files: %zu; %zu error(s), %zu warning(s)%s\n", totalAscii, errors, warnings,
                strict ? " (strict: warnings are errors)" : "");
    if (errors > 0) return 1;
    if (strict && warnings > 0) return 1;
    return 0;
}

} // namespace pxc::tool
