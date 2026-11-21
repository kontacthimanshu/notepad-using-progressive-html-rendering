#include "pxc/layout/layout_engine.hpp"

#include <algorithm>
#include <limits>

namespace pxc::layout {

using document::TextDocument;

namespace {
std::size_t bytesOf(const LayoutLine& l) {
    std::size_t b = sizeof(LayoutLine) + l.rows.size() * sizeof(VisualRow);
    for (const auto& r : l.rows) b += r.glyphs.size() * sizeof(PositionedGlyph);
    return b;
}
bool isBreakSpace(char32_t c) { return c == U' ' || c == U'\t'; }
} // namespace

LayoutEngine::LayoutEngine(const fonts::FontRegistry& registry) : registry_(&registry) { setParams(LayoutParams{}); }

void LayoutEngine::setParams(const LayoutParams& p) {
    params_ = p;
    params_.scale = std::max(1, p.scale);
    const auto fm = registry_->metrics(params_.packId);
    const int s = params_.scale;
    rowHeight_ = std::max(1, fm.lineHeight) * s;
    ascent_ = fm.ascent * s;
    spaceAdvance_ = std::max(1, fm.spaceAdvance) * s;
    int sum = 0, n = 0;
    for (char32_t c = U'a'; c <= U'z'; ++c) {
        auto rg = registry_->resolve(params_.packId, c);
        if (rg.def) { sum += rg.def->advance; ++n; }
    }
    avgAdvance_ = std::max(1, (n ? sum / n : fm.spaceAdvance * 2) * s);
    packRevision_ = registry_->packRevision(params_.packId);
    invalidateAll();
}

void LayoutEngine::reset(const TextDocument& doc) {
    doc_ = &doc;
    invalidateAll();
}

void LayoutEngine::invalidateAll() {
    lru_.clear();
    cache_.clear();
    cacheBytes_ = 0;
    maxWidth_ = 0;
    idleCursor_ = 0;
    if (!doc_) {
        metrics_.assign(1, LineMetrics{1, rowHeight_, false});
        unmeasured_ = 1;
        rebuildHeights();
        return;
    }
    const std::size_t n = doc_->lineCount();
    metrics_.resize(n);
    for (std::size_t i = 0; i < n; ++i) metrics_[i] = estimate(*doc_, i);
    unmeasured_ = n;
    rebuildHeights();
}

LineMetrics LayoutEngine::estimate(const TextDocument& doc, std::size_t line) const {
    LineMetrics m;
    m.exact = false;
    if (params_.wrapWidth > 0) {
        const std::int64_t px = static_cast<std::int64_t>(doc.lineLength(line)) * avgAdvance_;
        m.rows = static_cast<std::uint32_t>(std::max<std::int64_t>(1, (px + params_.wrapWidth - 1) / params_.wrapWidth));
    } else {
        m.rows = 1;
    }
    m.height = static_cast<std::int32_t>(m.rows) * rowHeight_;
    return m;
}

void LayoutEngine::rebuildHeights() {
    std::vector<std::int64_t> h(metrics_.size());
    for (std::size_t i = 0; i < metrics_.size(); ++i) h[i] = metrics_[i].height;
    heights_.assign(h);
}

int LayoutEngine::advanceOf(char32_t cp, int penX) const {
    if (cp == U'\t') {
        const int stop = std::max(1, params_.tabCells * spaceAdvance_);
        return (penX / stop + 1) * stop - penX;
    }
    auto rg = registry_->resolve(params_.packId, cp);
    if (rg.def) return rg.def->advance * params_.scale;
    return avgAdvance_; // unresolved code point (no fallback glyph available)
}

void LayoutEngine::wrapFrom(LayoutLine& out, const std::u32string& text, std::size_t startCol, int startY,
                            const LayoutLine* old, std::size_t editEndColNew, std::ptrdiff_t delta) const {
    const std::size_t n = text.size();
    const int wrap = params_.wrapWidth;
    std::size_t col = startCol;
    int y = startY;
    do {
        VisualRow row;
        row.startCol = col;
        row.y = y;
        row.height = rowHeight_;
        int x = 0;
        std::size_t lastSpaceEnd = std::numeric_limits<std::size_t>::max();
        std::size_t c = col;
        while (c < n) {
            const char32_t cp = text[c];
            const int adv = advanceOf(cp, x);
            if (wrap > 0 && x + adv > wrap && c > col && !isBreakSpace(cp)) {
                if (lastSpaceEnd != std::numeric_limits<std::size_t>::max() && lastSpaceEnd > col) {
                    row.glyphs.resize(lastSpaceEnd - col);
                    c = lastSpaceEnd;
                }
                break;
            }
            row.glyphs.push_back({c, cp, x, adv});
            x += adv;
            if (isBreakSpace(cp)) lastSpaceEnd = c + 1;
            ++c;
        }
        row.endCol = c;
        row.width = row.glyphs.empty() ? 0 : row.glyphs.back().x + row.glyphs.back().advance;
        int ink = 0;
        for (const auto& g : row.glyphs) {
            auto rg = registry_->resolve(params_.packId, g.cp);
            const int right = rg.def ? g.x + (rg.def->bearingX + rg.def->width) * params_.scale : g.x + g.advance;
            ink = std::max(ink, std::max(right, g.x + g.advance));
        }
        row.inkRight = ink;
        out.rows.push_back(std::move(row));
        counters_.rowsRecomputed++;
        y += rowHeight_;
        col = c;

        // Resynchronise with the old layout once past the edited region (research R-13).
        if (old && col > editEndColNew && col < n) {
            const auto oldStart = static_cast<std::ptrdiff_t>(col) - delta;
            for (std::size_t k = 0; k < old->rows.size(); ++k) {
                if (static_cast<std::ptrdiff_t>(old->rows[k].startCol) == oldStart) {
                    for (std::size_t j = k; j < old->rows.size(); ++j) {
                        VisualRow r = old->rows[j];
                        r.startCol = static_cast<std::size_t>(static_cast<std::ptrdiff_t>(r.startCol) + delta);
                        r.endCol = static_cast<std::size_t>(static_cast<std::ptrdiff_t>(r.endCol) + delta);
                        for (auto& g : r.glyphs) g.col = static_cast<std::size_t>(static_cast<std::ptrdiff_t>(g.col) + delta);
                        r.y = y;
                        y += r.height;
                        out.rows.push_back(std::move(r));
                    }
                    return;
                }
                if (static_cast<std::ptrdiff_t>(old->rows[k].startCol) > oldStart) break;
            }
        }
    } while (col < n);
}

LayoutLine LayoutEngine::build(const TextDocument& doc, std::size_t line) const {
    LayoutLine out;
    out.lineRevision = doc.lineRevision(line);
    const std::u32string text = doc.lineText(line);
    wrapFrom(out, text, 0, 0, nullptr, 0, 0);
    for (const auto& r : out.rows) {
        out.height += r.height;
        out.width = std::max(out.width, r.width);
    }
    counters_.linesLaidOut++;
    return out;
}

void LayoutEngine::setMetrics(std::size_t line, const LayoutLine& l) {
    if (line >= metrics_.size()) return;
    LineMetrics& m = metrics_[line];
    if (!m.exact && unmeasured_ > 0) --unmeasured_;
    const std::int64_t d = static_cast<std::int64_t>(l.height) - m.height;
    m = LineMetrics{static_cast<std::uint32_t>(l.rows.size()), l.height, true};
    if (d != 0) heights_.add(line, d);
    maxWidth_ = std::max(maxWidth_, l.width);
}

const LayoutLine& LayoutEngine::store(LayoutLine&& l) {
    const std::uint64_t rev = l.lineRevision;
    if (auto it = cache_.find(rev); it != cache_.end()) {
        cacheBytes_ -= it->second->bytes;
        lru_.erase(it->second);
        cache_.erase(it);
    }
    Entry e;
    e.bytes = bytesOf(l);
    e.line = std::move(l);
    cacheBytes_ += e.bytes;
    lru_.push_front(std::move(e));
    cache_[rev] = lru_.begin();
    while ((cache_.size() > kFullTierMaxLines || cacheBytes_ > kFullTierMaxBytes) && lru_.size() > 1) {
        cacheBytes_ -= lru_.back().bytes;
        cache_.erase(lru_.back().line.lineRevision);
        lru_.pop_back();
    }
    return lru_.front().line;
}

const LayoutLine& LayoutEngine::line(const TextDocument& doc, std::size_t ln) {
    if (doc_ != &doc) reset(doc);
    ln = std::min(ln, doc.lineCount() - 1);
    const std::uint64_t rev = doc.lineRevision(ln);
    if (auto it = cache_.find(rev); it != cache_.end()) {
        lru_.splice(lru_.begin(), lru_, it->second);
        const LayoutLine& l = it->second->line;
        if (ln < metrics_.size() && (!metrics_[ln].exact || metrics_[ln].height != l.height)) setMetrics(ln, l);
        return l;
    }
    LayoutLine l = build(doc, ln);
    setMetrics(ln, l);
    return store(std::move(l));
}

LineMetrics LayoutEngine::metrics(std::size_t line) const {
    return line < metrics_.size() ? metrics_[line] : LineMetrics{};
}

std::pair<std::size_t, std::size_t> LayoutEngine::onEdit(const TextDocument& doc, const document::EditResult& e) {
    constexpr auto kAll = std::numeric_limits<std::size_t>::max();
    if (!e.valid) return {0, 0};
    if (doc_ != &doc) {
        reset(doc);
        return {0, kAll};
    }
    // Splice the metrics tier.
    if (e.oldLineCount == e.newLineCount) {
        for (std::size_t k = 0; k < e.newLineCount; ++k) {
            const std::size_t ln = e.firstLine + k;
            LineMetrics est = estimate(doc, ln);
            if (metrics_[ln].exact) ++unmeasured_;
            const std::int64_t d = static_cast<std::int64_t>(est.height) - metrics_[ln].height;
            metrics_[ln] = est;
            if (d != 0) heights_.add(ln, d);
        }
    } else {
        for (std::size_t k = 0; k < e.oldLineCount && e.firstLine + k < metrics_.size(); ++k)
            if (!metrics_[e.firstLine + k].exact && unmeasured_ > 0) --unmeasured_;
        metrics_.erase(metrics_.begin() + static_cast<std::ptrdiff_t>(e.firstLine),
                       metrics_.begin() + static_cast<std::ptrdiff_t>(std::min(metrics_.size(), e.firstLine + e.oldLineCount)));
        std::vector<LineMetrics> fresh;
        for (std::size_t k = 0; k < e.newLineCount; ++k) fresh.push_back(estimate(doc, e.firstLine + k));
        metrics_.insert(metrics_.begin() + static_cast<std::ptrdiff_t>(e.firstLine), fresh.begin(), fresh.end());
        unmeasured_ += e.newLineCount;
        rebuildHeights();
        return {0, kAll};
    }
    if (e.oldLineCount != 1) return {0, kAll};

    // Single-line edit: incremental re-layout from the edited visual row.
    auto it = cache_.find(e.oldFirstLineRevision);
    if (it == cache_.end()) return {0, kAll};
    const LayoutLine old = it->second->line; // copy: store() may evict the original
    const std::u32string text = doc.lineText(e.firstLine);
    const std::ptrdiff_t delta = static_cast<std::ptrdiff_t>(e.inserted) - static_cast<std::ptrdiff_t>(e.removed);
    const std::size_t editCol = e.firstLineColumn;
    LayoutLine out;
    out.lineRevision = doc.lineRevision(e.firstLine);
    ++counters_.incrementalRelayouts;
    ++counters_.linesLaidOut;
    std::size_t firstRow = 0;

    bool hasTabAfter = false;
    for (std::size_t c = editCol; c < text.size(); ++c)
        if (text[c] == U'\t') { hasTabAfter = true; break; }

    if (params_.wrapWidth == 0 && old.rows.size() == 1 && !hasTabAfter) {
        // No wrap: splice the glyph vector and shift later x positions.
        VisualRow row = old.rows[0];
        auto& gl = row.glyphs;
        const std::size_t removeEnd = std::min(gl.size(), editCol + e.removed);
        int removedWidth = 0;
        for (std::size_t k = editCol; k < removeEnd; ++k) removedWidth += gl[k].advance;
        const int x0 = editCol < gl.size() ? gl[editCol].x : row.width;
        gl.erase(gl.begin() + static_cast<std::ptrdiff_t>(std::min(editCol, gl.size())),
                 gl.begin() + static_cast<std::ptrdiff_t>(removeEnd));
        std::vector<PositionedGlyph> ins;
        int x = x0;
        for (std::size_t k = 0; k < e.inserted; ++k) {
            const char32_t cp = text[editCol + k];
            const int adv = advanceOf(cp, x);
            ins.push_back({editCol + k, cp, x, adv});
            x += adv;
        }
        const int dx = (x - x0) - removedWidth;
        gl.insert(gl.begin() + static_cast<std::ptrdiff_t>(std::min(editCol, gl.size())), ins.begin(), ins.end());
        for (std::size_t k = editCol + e.inserted; k < gl.size(); ++k) {
            gl[k].x += dx;
            gl[k].col = static_cast<std::size_t>(static_cast<std::ptrdiff_t>(gl[k].col) + delta);
        }
        row.endCol = text.size();
        row.width = gl.empty() ? 0 : gl.back().x + gl.back().advance;
        row.inkRight = std::max(row.inkRight + dx, row.width);
        ++counters_.rowsRecomputed;
        out.rows.push_back(std::move(row));
    } else {
        // Wrap: copy rows before the edit (starting one row early, since deletions may pull words back).
        std::size_t r = 0;
        while (r + 1 < old.rows.size() && old.rows[r + 1].startCol <= editCol) ++r;
        firstRow = r > 0 ? r - 1 : 0;
        for (std::size_t k = 0; k < firstRow; ++k) out.rows.push_back(old.rows[k]);
        const std::size_t startCol = old.rows[firstRow].startCol;
        const int startY = old.rows[firstRow].y;
        wrapFrom(out, text, startCol, startY, &old, editCol + e.inserted, delta);
    }
    for (const auto& r : out.rows) {
        out.height += r.height;
        out.width = std::max(out.width, r.width);
    }
    const bool sameRowCount = out.rows.size() == old.rows.size();
    std::size_t lastChanged = out.rows.size() - 1;
    if (sameRowCount) {
        // Rows after the resync point are unchanged except for column numbering.
        for (std::size_t k = out.rows.size(); k-- > firstRow;) {
            const auto& a = out.rows[k];
            const auto& b = old.rows[k];
            if (static_cast<std::ptrdiff_t>(a.startCol) - delta != static_cast<std::ptrdiff_t>(b.startCol) ||
                a.width != b.width || a.glyphs.size() != b.glyphs.size()) {
                lastChanged = k;
                break;
            }
            lastChanged = k;
        }
    }
    setMetrics(e.firstLine, out);
    store(std::move(out));
    if (!sameRowCount) return {firstRow, kAll};
    return {firstRow, std::max(firstRow, lastChanged)};
}

std::int64_t LayoutEngine::lineTop(std::size_t line) const { return heights_.prefix(line); }
std::int64_t LayoutEngine::totalHeight() const { return heights_.total(); }

std::size_t LayoutEngine::lineAtY(std::int64_t docY) const {
    if (docY <= 0 || metrics_.empty()) return 0;
    const std::size_t i = heights_.upperIndex(docY);
    return std::min(i, metrics_.size() - 1);
}

CaretGeometry LayoutEngine::caretAt(const TextDocument& doc, std::size_t offset) {
    const auto lc = doc.toLineCol(offset);
    const LayoutLine& l = line(doc, lc.line);
    CaretGeometry g;
    g.line = lc.line;
    std::size_t r = 0;
    for (std::size_t k = 0; k < l.rows.size(); ++k) {
        if (lc.col >= l.rows[k].startCol && (lc.col < l.rows[k].endCol || k + 1 == l.rows.size())) {
            r = k;
            break;
        }
    }
    const VisualRow& row = l.rows[r];
    g.row = r;
    if (lc.col < row.endCol && lc.col >= row.startCol && lc.col - row.startCol < row.glyphs.size())
        g.x = row.glyphs[lc.col - row.startCol].x;
    else
        g.x = row.width;
    g.y = static_cast<int>(lineTop(lc.line)) + row.y;
    g.height = row.height;
    return g;
}

std::size_t LayoutEngine::offsetInRow(const TextDocument& doc, std::size_t ln, std::size_t row, int x) {
    const LayoutLine& l = line(doc, ln);
    row = std::min(row, l.rows.size() - 1);
    const VisualRow& r = l.rows[row];
    const std::size_t base = doc.lineStart(ln);
    for (const auto& g : r.glyphs)
        if (x < g.x + g.advance / 2) return base + g.col;
    if (row + 1 < l.rows.size() && r.endCol > r.startCol) return base + r.endCol - 1;
    return base + r.endCol;
}

std::size_t LayoutEngine::hitTest(const TextDocument& doc, int docX, std::int64_t docY) {
    if (docY < 0) return 0;
    if (docY >= totalHeight()) {
        const std::size_t last = doc.lineCount() - 1;
        const LayoutLine& l = line(doc, last);
        return offsetInRow(doc, last, l.rows.size() - 1, docX);
    }
    const std::size_t ln = lineAtY(docY);
    const LayoutLine& l = line(doc, ln); // may refine metrics; recompute local y afterwards
    const std::int64_t localY = docY - lineTop(ln);
    std::size_t row = 0;
    for (std::size_t k = 0; k < l.rows.size(); ++k)
        if (l.rows[k].y <= localY) row = k;
    return offsetInRow(doc, ln, row, docX);
}

bool LayoutEngine::layoutIdleSlice(const TextDocument& doc, std::chrono::microseconds budget, std::size_t focusLine) {
    if (doc_ != &doc) reset(doc);
    if (unmeasured_ == 0) return false;
    const auto start = std::chrono::steady_clock::now();
    const std::size_t n = metrics_.size();
    if (idleCursor_ == 0 || idleCursor_ >= n) idleCursor_ = std::min(focusLine, n - 1);
    std::size_t scanned = 0;
    while (unmeasured_ > 0 && scanned < n) {
        const std::size_t ln = idleCursor_;
        idleCursor_ = (idleCursor_ + 1) % n;
        ++scanned;
        if (metrics_[ln].exact) continue;
        LayoutLine l = build(doc, ln);
        setMetrics(ln, l);
        if (cache_.size() < kFullTierMaxLines / 2) store(std::move(l));
        if ((scanned & 15) == 0 && std::chrono::steady_clock::now() - start >= budget) break;
    }
    return unmeasured_ > 0;
}

} // namespace pxc::layout
