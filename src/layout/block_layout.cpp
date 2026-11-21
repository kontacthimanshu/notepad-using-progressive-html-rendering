#include "pxc/layout/block_layout.hpp"

#include <algorithm>
#include <cmath>

namespace pxc::layout {

namespace {

struct RunMetrics {
    int lineHeight = 0; // device px
    int ascent = 0;     // device px including half-leading
};

RunMetrics metricsFor(const fonts::FontRegistry& reg, const StyledRun& r, int s) {
    const auto fm = reg.metrics(r.packId);
    const int natural = fm.lineHeight * s;
    int lh = natural;
    if (r.lineHeightPx > 0) lh = static_cast<int>(std::lround(r.lineHeightPx * s));
    else if (r.lineHeightMultiplier > 0) lh = static_cast<int>(std::lround(r.lineHeightMultiplier * fm.lineHeight * s));
    lh = std::max(lh, 1);
    RunMetrics m;
    m.lineHeight = lh;
    m.ascent = (lh - natural) / 2 + fm.ascent * s;
    return m;
}

const core::Color kPage{255, 255, 255, 255};

} // namespace

PreviewLayout layoutBlocks(const std::vector<StyledBlock>& blocks, const fonts::FontRegistry& registry, int width,
                           int scale, int marginLeft) {
    PreviewLayout out;
    const int s = std::max(1, scale);
    out.width = width;
    const int wrap = std::max(1, width - 2 * marginLeft);
    auto packIndex = [&](const std::string& id) -> std::uint16_t {
        for (std::size_t i = 0; i < out.packs.size(); ++i)
            if (out.packs[i] == id) return static_cast<std::uint16_t>(i);
        out.packs.push_back(id);
        return static_cast<std::uint16_t>(out.packs.size() - 1);
    };

    int y = 0;
    int prevMarginBottom = 0;
    bool firstBlock = true;
    for (const auto& b : blocks) {
        y += std::max(prevMarginBottom, b.marginTopPx) * s;
        if (!firstBlock) out.text += U'\n';
        firstBlock = false;
        const int blockTop = y;

        struct Pending {
            PreviewGlyph g;
            std::size_t run;
        };
        std::vector<Pending> row;
        int x = 0;
        std::size_t lastSpaceEnd = 0; // index into row after the last space (0 = none)
        RunMetrics emptyRowMetrics = b.runs.empty() ? RunMetrics{15 * s, 11 * s} : metricsFor(registry, b.runs[0], s);

        auto finishRow = [&](std::size_t upto, std::size_t forcedRun) {
            PreviewRow r;
            r.y = y;
            int lh = 0, asc = 0;
            for (std::size_t k = 0; k < upto; ++k) {
                const auto m = metricsFor(registry, b.runs[row[k].run], s);
                lh = std::max(lh, m.lineHeight);
                asc = std::max(asc, m.ascent);
            }
            if (upto == 0) {
                const auto m = forcedRun < b.runs.size() ? metricsFor(registry, b.runs[forcedRun], s) : emptyRowMetrics;
                lh = m.lineHeight;
                asc = m.ascent;
            }
            r.height = lh;
            r.baseline = asc;
            for (std::size_t k = 0; k < upto; ++k) {
                PreviewGlyph g = row[k].g;
                g.x += marginLeft;
                if (g.bg.a > 0 && !(g.bg == kPage) && g.cp != U'\n') out.backgrounds.push_back({{g.x, y, g.advance, lh}, g.bg});
                r.glyphs.push_back(g);
            }
            out.rows.push_back(std::move(r));
            y += lh;
            // Carry the glyphs after the break onto the next row.
            std::vector<Pending> rest(row.begin() + static_cast<std::ptrdiff_t>(upto), row.end());
            const int shift = rest.empty() ? 0 : rest.front().g.x;
            for (auto& p : rest) p.g.x -= shift;
            row = std::move(rest);
            x = row.empty() ? 0 : row.back().g.x + row.back().g.advance;
            lastSpaceEnd = 0;
            for (std::size_t k = 0; k < row.size(); ++k)
                if (row[k].g.cp == U' ') lastSpaceEnd = k + 1;
        };

        for (std::size_t ri = 0; ri < b.runs.size(); ++ri) {
            const StyledRun& run = b.runs[ri];
            const std::uint16_t pi = packIndex(run.packId);
            const core::Color bg = run.bg.a > 0 ? run.bg : (b.background.a > 0 ? b.background : kPage);
            for (char32_t cp : run.text) {
                const std::size_t offset = out.text.size();
                out.text += cp;
                if (cp == U'\n') {
                    finishRow(row.size(), ri);
                    continue;
                }
                const auto rg = registry.resolve(run.packId, cp);
                const int adv = rg.def ? rg.def->advance * s : 4 * s;
                if (x + adv > wrap && !row.empty() && cp != U' ') {
                    finishRow(lastSpaceEnd > 0 ? lastSpaceEnd : row.size(), ri);
                }
                PreviewGlyph g;
                g.offset = offset;
                g.cp = cp;
                g.pack = pi;
                g.x = x;
                g.advance = adv;
                g.fg = run.fg;
                g.bg = bg;
                row.push_back({g, ri});
                x += adv;
                if (cp == U' ') lastSpaceEnd = row.size();
            }
        }
        if (!row.empty()) finishRow(row.size(), b.runs.size());
        if (b.background.a > 0 && y > blockTop)
            out.backgrounds.insert(out.backgrounds.begin(), PreviewBackground{{0, blockTop, width, y - blockTop}, b.background});
        prevMarginBottom = b.marginBottomPx;
    }
    out.height = y + prevMarginBottom * s;
    return out;
}

std::size_t previewHitTest(const PreviewLayout& l, int x, int y) {
    if (l.rows.empty()) return 0;
    const PreviewRow* row = &l.rows.front();
    for (const auto& r : l.rows)
        if (r.y <= y) row = &r;
    if (y < 0) row = &l.rows.front();
    for (const auto& g : row->glyphs)
        if (x < g.x + g.advance / 2) return g.offset;
    if (!row->glyphs.empty()) return row->glyphs.back().offset + 1;
    // Empty row: the offset just after the previous row's last glyph.
    std::size_t best = 0;
    for (const auto& r : l.rows) {
        if (&r == row) break;
        if (!r.glyphs.empty()) best = r.glyphs.back().offset + 1;
    }
    return best;
}

} // namespace pxc::layout
