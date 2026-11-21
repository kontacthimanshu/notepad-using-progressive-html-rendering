// `distinct`: evidence that the packs are independently designed, not resized copies (FR-009).
// For every pair of packs and every shared inked code point, both glyphs are reduced to an ink bitmap
// (literal or currentcolor = 1) cropped to the ink bounding box. Different sizes count as distinct;
// equal sizes are compared cell by cell. A pair fails when more than 60% of shared glyphs are identical.
#include <cstdio>
#include <filesystem>
#include <map>
#include <vector>

#include "pxc/fonts/font_registry.hpp"
#include "tool.hpp"

namespace pxc::tool {

namespace {

struct Ink {
    int w = 0, h = 0;
    std::vector<bool> bits;
    bool operator==(const Ink&) const = default;
};

Ink inkOf(const glyphcss::GlyphDefinition& g) {
    using glyphcss::CellKind;
    int x0 = g.width, y0 = g.height, x1 = -1, y1 = -1;
    auto inked = [&](int c, int r) {
        const auto k = g.cellAt(c, r).kind;
        return k == CellKind::Literal || k == CellKind::CurrentColor;
    };
    for (int r = 0; r < g.height; ++r)
        for (int c = 0; c < g.width; ++c)
            if (inked(c, r)) {
                x0 = std::min(x0, c); y0 = std::min(y0, r);
                x1 = std::max(x1, c); y1 = std::max(y1, r);
            }
    Ink ink;
    if (x1 < 0) return ink;
    ink.w = x1 - x0 + 1;
    ink.h = y1 - y0 + 1;
    for (int r = y0; r <= y1; ++r)
        for (int c = x0; c <= x1; ++c) ink.bits.push_back(inked(c, r));
    return ink;
}

} // namespace

int cmdDistinct(const Args& a) {
    if (a.positional.size() != 1) {
        std::fputs("distinct: expected <fonts-dir>\n", stderr);
        return 2;
    }
    core::DiagnosticsLog log;
    fonts::FontRegistry reg(log);
    reg.loadAll(a.positional[0]);
    std::vector<std::string> ids;
    for (const auto& p : reg.packs())
        if (p.state != fonts::PackState::Invalid) ids.push_back(p.id);
    if (ids.size() < 2) {
        std::fputs("distinct: need at least two usable packs\n", stderr);
        return 2;
    }
    std::map<std::string, std::map<char32_t, Ink>> inks;
    for (const auto& id : ids)
        for (const auto* g : reg.glyphs(id))
            if (g->hasInk()) inks[id][g->codePoint] = inkOf(*g);

    bool fail = false;
    std::printf("%-22s %-22s %8s %10s %10s\n", "pack A", "pack B", "shared", "identical", "percent");
    for (std::size_t i = 0; i < ids.size(); ++i)
        for (std::size_t j = i + 1; j < ids.size(); ++j) {
            std::size_t shared = 0, identical = 0;
            for (const auto& [cp, ink] : inks[ids[i]]) {
                auto other = inks[ids[j]].find(cp);
                if (other == inks[ids[j]].end()) continue;
                ++shared;
                if (ink == other->second) ++identical;
            }
            const double pct = shared ? 100.0 * static_cast<double>(identical) / static_cast<double>(shared) : 0.0;
            const bool bad = pct > 60.0;
            fail |= bad;
            std::printf("%-22s %-22s %8zu %10zu %9.1f%%%s\n", ids[i].c_str(), ids[j].c_str(), shared, identical, pct,
                        bad ? "  FAIL (> 60%)" : "");
        }
    std::puts(fail ? "result: NOT distinct" : "result: all pack pairs are distinct");
    return fail ? 1 : 0;
}

} // namespace pxc::tool
