#include "pxc/update/update_manager.hpp"

#include <algorithm>
#include <chrono>
#include <limits>

namespace pxc::update {

using core::Rect;

UpdateManager::UpdateManager(const document::TextDocument& doc, layout::LayoutEngine& layout,
                             fonts::RasterCache& cache, layout::Viewport& viewport)
    : doc_(&doc), layout_(&layout), cache_(&cache), viewport_(&viewport) {
    dirty_.setBounds({0, 0, viewport.clientWidth, viewport.clientHeight});
    dirty_.markFull();
}

// --- notifications (research R-13) ---

void UpdateManager::addSelectionDelta(const PaintState& before, const PaintState& after) {
    const bool b = before.selEnd > before.selStart, a = after.selEnd > after.selStart;
    const std::size_t len = doc_->length();
    auto clampOff = [&](std::size_t o) { return std::min(o, len); };
    if (b && a) {
        // Only the parts whose selection state changed.
        if (before.selStart != after.selStart)
            addRangeRects(clampOff(std::min(before.selStart, after.selStart)), clampOff(std::max(before.selStart, after.selStart)));
        if (before.selEnd != after.selEnd)
            addRangeRects(clampOff(std::min(before.selEnd, after.selEnd)), clampOff(std::max(before.selEnd, after.selEnd)));
    } else if (b) {
        addRangeRects(clampOff(before.selStart), clampOff(before.selEnd));
    } else if (a) {
        addRangeRects(clampOff(after.selStart), clampOff(after.selEnd));
    }
}

void UpdateManager::onEdit(const document::EditResult& e, std::pair<std::size_t, std::size_t> rows,
                           const PaintState& before, const PaintState& after) {
    constexpr auto kAll = std::numeric_limits<std::size_t>::max();
    if (!e.valid) return;
    dirty_.add(caretRect(std::min(before.caret, doc_->length())));
    dirty_.add(caretRect(after.caret));
    addSelectionDelta(before, after);
    const std::int64_t sy = viewport_->scrollY(*layout_);
    const int s = std::max(1, layout_->params().scale);
    const auto editGeom = layout_->caretAt(*doc_, std::min(e.offset, doc_->length()));
    if (e.oldLineCount != e.newLineCount || rows.second == kAll) {
        // Rows from the edited one down to the viewport bottom shift or re-wrap.
        const auto& line = layout_->line(*doc_, e.firstLine);
        const std::size_t first = std::min(rows.first, line.rows.size() - 1);
        const int top = static_cast<int>(layout_->lineTop(e.firstLine) - sy) + line.rows[first].y;
        dirty_.add({0, std::min(top, static_cast<int>(editGeom.y - sy)), viewport_->clientWidth, viewport_->clientHeight});
        return;
    }
    const auto& line = layout_->line(*doc_, e.firstLine);
    const int lineTop = static_cast<int>(layout_->lineTop(e.firstLine) - sy);
    if (rows.first == rows.second && rows.first == editGeom.row) {
        // A single visual row changed: from the edit position to the right edge of that row.
        const auto& row = line.rows[rows.first];
        const int x = viewport_->toViewX(editGeom.x) - 2 * s;
        dirty_.add({x, lineTop + row.y, viewport_->clientWidth - x, row.height});
        return;
    }
    for (std::size_t r = rows.first; r <= rows.second && r < line.rows.size(); ++r)
        dirty_.add({0, lineTop + line.rows[r].y, viewport_->clientWidth, line.rows[r].height});
}

void UpdateManager::onCaretMoved(const PaintState& before, const PaintState& after) {
    dirty_.add(caretRect(std::min(before.caret, doc_->length())));
    dirty_.add(caretRect(after.caret));
    addSelectionDelta(before, after);
}

void UpdateManager::onFontChanged() { dirty_.markFull(); }
void UpdateManager::onWrapToggled() { dirty_.markFull(); }
void UpdateManager::onDpiChanged(int) { dirty_.markFull(); }
void UpdateManager::onResize(int w, int h) {
    dirty_.setBounds({0, 0, w, h});
    dirty_.markFull();
}
void UpdateManager::onScroll(std::int64_t oldScrollY, int oldScrollX) {
    if (viewport_->scrollX != oldScrollX) {
        dirty_.markFull();
        pendingScrollDy_ = 0;
        return;
    }
    const std::int64_t dy = viewport_->scrollY(*layout_) - oldScrollY;
    if (dy == 0) return;
    pendingScrollDy_ += dy;
    dirty_.shiftY(static_cast<int>(-std::clamp<std::int64_t>(dy, -(1 << 28), 1 << 28)));
}
void UpdateManager::onDocumentReplaced() {
    dirty_.markFull();
    pendingScrollDy_ = 0;
}

core::Rect UpdateManager::caretRect(std::size_t offset) {
    const auto g = layout_->caretAt(*doc_, offset);
    const int s = std::max(1, layout_->params().scale);
    const int y = static_cast<int>(g.y - viewport_->scrollY(*layout_));
    return {viewport_->toViewX(g.x) - s, y, 3 * s, g.height};
}

void UpdateManager::addRangeRects(std::size_t from, std::size_t to) {
    if (from > to) std::swap(from, to);
    const auto a = layout_->caretAt(*doc_, from);
    const auto b = layout_->caretAt(*doc_, to);
    const std::int64_t sy = viewport_->scrollY(*layout_);
    const int top = static_cast<int>(a.y - sy);
    const int bottom = static_cast<int>(b.y - sy) + b.height;
    dirty_.add({0, top, viewport_->clientWidth, bottom - top});
}

void UpdateManager::addRowsFrom(std::size_t line, std::size_t row) {
    const std::int64_t sy = viewport_->scrollY(*layout_);
    const auto& l = layout_->line(*doc_, line);
    const int top = static_cast<int>(layout_->lineTop(line) - sy) + l.rows[std::min(row, l.rows.size() - 1)].y;
    dirty_.add({0, top, viewport_->clientWidth, viewport_->clientHeight - top});
}

// --- painting ---

void UpdateManager::paintRect(render::Framebuffer& fb, Rect clip, const PaintState& state) {
    clip = clip.intersect(fb.bounds());
    if (clip.empty()) return;
    renderer_.fill(fb, clip, theme_.background);
    const auto& params = layout_->params();
    const int s = params.scale;
    const int ascent = layout_->ascentPx();
    const auto rows = viewport_->visibleRows(*layout_, *doc_);
    const bool hasSel = state.selEnd > state.selStart;

    for (const auto& vr : rows) {
        const auto& line = layout_->line(*doc_, vr.line);
        const auto& row = line.rows[vr.row];
        const Rect rowRect{0, vr.y, fb.width(), row.height};
        const Rect rowClip = rowRect.intersect(clip);
        if (rowClip.empty()) continue;
        const std::size_t lineStart = doc_->lineStart(vr.line);
        const std::size_t rowStart = lineStart + row.startCol;
        const std::size_t rowEnd = lineStart + row.endCol;
        const bool lastRow = vr.row + 1 == line.rows.size();
        const bool hasBreak = lastRow && vr.line + 1 < doc_->lineCount();

        // Selection background. The line break (at offset rowEnd) is selected only when the selection
        // actually covers that character.
        const bool breakSelected = hasSel && hasBreak && state.selStart <= rowEnd && state.selEnd > rowEnd;
        if (hasSel && (state.selStart < rowEnd || breakSelected) && state.selEnd > rowStart) {
            const std::size_t a = std::max(state.selStart, rowStart);
            const std::size_t b = std::min(state.selEnd, rowEnd);
            auto xAt = [&](std::size_t off) {
                const std::size_t c = off - lineStart;
                if (c >= row.startCol && c < row.endCol && c - row.startCol < row.glyphs.size())
                    return row.glyphs[c - row.startCol].x;
                return row.width;
            };
            int x0 = xAt(a), x1 = xAt(b);
            if (breakSelected) x1 += layout_->spaceAdvancePx(); // selected line break
            if (x1 > x0)
                renderer_.fill(fb, Rect{viewport_->toViewX(x0), vr.y, x1 - x0, row.height}.intersect(rowClip),
                               theme_.selectionFill);
        }

        // Glyphs.
        const int baseline = vr.y + ascent;
        for (const auto& g : row.glyphs) {
            const int vx = viewport_->toViewX(g.x);
            if (vx + g.advance + 16 * s < rowClip.x || vx - 16 * s > rowClip.right()) continue;
            if (g.cp == U'\t' || g.cp == U' ') continue;
            const std::size_t off = lineStart + g.col;
            const bool sel = hasSel && off >= state.selStart && off < state.selEnd;
            const core::Color fg = sel ? theme_.selectionText : theme_.text;
            const core::Color bg = sel ? theme_.selectionFill : theme_.background;
            const auto& raster = cache_->get(params.packId, g.cp, s, fg, bg);
            renderer_.blitGlyph(fb, raster, vx, baseline, rowClip);
        }
    }

    // Caret.
    if (state.caretVisible) {
        const auto g = layout_->caretAt(*doc_, state.caret);
        const int y = static_cast<int>(g.y - viewport_->scrollY(*layout_));
        renderer_.drawCaret(fb, Rect{viewport_->toViewX(g.x), y, std::max(1, s), g.height}.intersect(clip),
                            theme_.caret);
    }
}

RenderStats UpdateManager::paint(render::Framebuffer& fb, const PaintState& state, IRenderObserver* observer,
                                 std::vector<core::Rect>* presented) {
    RenderStats st;
    st.frame = ++frame_;
    if (dirty_.empty() && pendingScrollDy_ == 0) return st;
    using clock = std::chrono::steady_clock;
    const auto t0 = clock::now();
    const auto cacheBefore = cache_->stats();
    const auto layoutBefore = layout_->counters();
    renderer_.counters().reset();

    dirty_.setBounds(fb.bounds());
    // Scrolling: move the already-rendered pixels and repaint only the newly exposed band (research R-13).
    if (pendingScrollDy_ != 0) {
        const std::int64_t dy = pendingScrollDy_;
        pendingScrollDy_ = 0;
        const int h = fb.height();
        if (!dirty_.full() && dy > -h && dy < h) {
            const int d = static_cast<int>(dy);
            if (d > 0) {
                for (int y = 0; y + d < h; ++y) std::copy(fb.row(y + d).begin(), fb.row(y + d).end(), fb.row(y).begin());
                dirty_.add({0, h - d, fb.width(), d});
            } else {
                for (int y = h - 1; y + d >= 0; --y) std::copy(fb.row(y + d).begin(), fb.row(y + d).end(), fb.row(y).begin());
                dirty_.add({0, 0, fb.width(), -d});
            }
            st.scrollShift = d;
        } else {
            dirty_.markFull();
        }
    }
    std::vector<Rect> rects;
    if (dirty_.full()) rects.push_back(fb.bounds());
    else rects.assign(dirty_.rects().begin(), dirty_.rects().end());
    st.fullRepaint = dirty_.full();
    for (const Rect& r : rects) {
        paintRect(fb, r, state);
        st.dirtyPixelArea += static_cast<std::uint64_t>(r.intersect(fb.bounds()).area());
    }
    st.dirtyRectCount = rects.size();
    if (presented) {
        *presented = rects;
        if (st.scrollShift != 0) *presented = {fb.bounds()};
    }
    dirty_.clear();
    lastPainted_ = state;

    const auto cacheAfter = cache_->stats();
    st.cacheHits = cacheAfter.hits - cacheBefore.hits;
    st.cacheMisses = cacheAfter.misses - cacheBefore.misses;
    st.rasterizations = cacheAfter.rasterizations - cacheBefore.rasterizations;
    const auto total = st.cacheHits + st.cacheMisses;
    st.cacheHitRate = total ? static_cast<double>(st.cacheHits) / static_cast<double>(total) : 1.0;
    st.linesLaidOut = layout_->counters().linesLaidOut - layoutBefore.linesLaidOut;
    st.rowsRecomputed = layout_->counters().rowsRecomputed - layoutBefore.rowsRecomputed;
    st.glyphBlits = renderer_.counters().glyphBlits;
    st.paintMicros = std::chrono::duration_cast<std::chrono::microseconds>(clock::now() - t0).count();
    if (observer) observer->onFrame(st);
    return st;
}

} // namespace pxc::update
