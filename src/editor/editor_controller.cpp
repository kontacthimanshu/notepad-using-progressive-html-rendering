#include "pxc/editor/editor_controller.hpp"

#include <algorithm>

namespace pxc::editor {

namespace {
bool isWordChar(char32_t c) {
    return (c >= U'0' && c <= U'9') || (c >= U'a' && c <= U'z') || (c >= U'A' && c <= U'Z') || c == U'_' || c >= 0x80;
}
bool isSpace(char32_t c) { return c == U' ' || c == U'\t'; }
template <class... Ts> struct Overloaded : Ts... { using Ts::operator()...; };
template <class... Ts> Overloaded(Ts...) -> Overloaded<Ts...>;
} // namespace

EditorController::EditorController(document::TextDocument& doc, layout::LayoutEngine& layout, layout::Viewport& viewport,
                                   update::UpdateManager& updates, IClipboard& clipboard)
    : doc_(&doc), layout_(&layout), viewport_(&viewport), updates_(&updates), clipboard_(&clipboard) {
    layout_->reset(doc);
    applyLayoutParams(layout_->params().scale);
}

update::PaintState EditorController::paintState() const {
    update::PaintState p;
    p.caret = state_.caret;
    p.selStart = state_.selStart();
    p.selEnd = state_.selEnd();
    return p;
}

void EditorController::applyLayoutParams(int scale) {
    scale_ = std::max(1, scale);
    layout::LayoutParams p = layout_->params();
    p.packId = state_.activePackId;
    p.scale = scale_;
    p.wrapWidth = state_.wordWrap ? std::max(1, viewport_->clientWidth - 2 * viewport_->marginLeft) : 0;
    if (!(p == layout_->params())) {
        layout_->setParams(p);
        layout_->reset(*doc_);
        state_.preferredX.reset();
        if (state_.wordWrap) viewport_->scrollX = 0;
        updates_->invalidateAll();
        ensureCaretVisible();
    }
}

void EditorController::loadText(std::u32string_view text) {
    doc_->setText(text);
    layout_->reset(*doc_);
    state_.caret = state_.anchor = 0;
    state_.preferredX.reset();
    viewport_->anchor = {};
    viewport_->scrollX = 0;
    updates_->onDocumentReplaced();
}

void EditorController::ensureCaretVisible() {
    viewport_->scrollToMakeVisible(*layout_, layout_->caretAt(*doc_, state_.caret));
}

void EditorController::moveCaret(std::size_t newCaret, bool extend, bool keepPreferredX) {
    const auto before = paintState();
    newCaret = std::min(newCaret, doc_->length());
    if (newCaret != state_.caret || (!extend && state_.hasSelection())) doc_->breakUndoGroup();
    state_.caret = newCaret;
    if (!extend) state_.anchor = newCaret;
    if (!keepPreferredX) state_.preferredX.reset();
    updates_->onCaretMoved(before, paintState());
    ensureCaretVisible();
}

std::size_t EditorController::wordBoundary(std::size_t from, int dir) const {
    const std::size_t n = doc_->length();
    std::size_t i = from;
    if (dir > 0) {
        if (i >= n) return n;
        const char32_t c = doc_->at(i);
        if (c == U'\n') return i + 1;
        if (isWordChar(c)) while (i < n && isWordChar(doc_->at(i))) ++i;
        else if (!isSpace(c)) ++i;
        while (i < n && isSpace(doc_->at(i))) ++i;
        return i;
    }
    if (i == 0) return 0;
    --i;
    while (i > 0 && isSpace(doc_->at(i))) --i;
    if (doc_->at(i) == U'\n') return i == from - 1 ? i : i + 1;
    if (isWordChar(doc_->at(i))) {
        while (i > 0 && isWordChar(doc_->at(i - 1))) --i;
    }
    return i;
}

std::size_t EditorController::moveTarget(MoveKind k) {
    const std::size_t c = state_.caret;
    const auto geom = layout_->caretAt(*doc_, c);
    const auto& line = layout_->line(*doc_, geom.line);
    const std::size_t lineStart = doc_->lineStart(geom.line);
    switch (k) {
    case MoveKind::Left:
        if (state_.hasSelection()) return state_.selStart();
        return c > 0 ? c - 1 : 0;
    case MoveKind::Right:
        if (state_.hasSelection()) return state_.selEnd();
        return std::min(c + 1, doc_->length());
    case MoveKind::Home: return lineStart + line.rows[geom.row].startCol;
    case MoveKind::End: {
        const auto& row = line.rows[geom.row];
        const bool last = geom.row + 1 == line.rows.size();
        return lineStart + (last || row.endCol == row.startCol ? row.endCol : row.endCol - 1);
    }
    case MoveKind::DocStart: return 0;
    case MoveKind::DocEnd: return doc_->length();
    case MoveKind::WordLeft: return wordBoundary(c, -1);
    case MoveKind::WordRight: return wordBoundary(c, +1);
    case MoveKind::Up:
    case MoveKind::Down:
    case MoveKind::PageUp:
    case MoveKind::PageDown: {
        const int x = state_.preferredX.value_or(geom.x);
        state_.preferredX = x;
        int rowsToMove = (k == MoveKind::Up || k == MoveKind::Down) ? 1 : viewport_->pageRows(*layout_);
        const int dir = (k == MoveKind::Up || k == MoveKind::PageUp) ? -1 : 1;
        std::size_t ln = geom.line, row = geom.row;
        while (rowsToMove-- > 0) {
            if (dir < 0) {
                if (row > 0) --row;
                else if (ln > 0) { --ln; row = layout_->line(*doc_, ln).rows.size() - 1; }
                else if (k == MoveKind::PageUp) {
                    state_.preferredX = 0; // jumped to the document start: column 0 becomes the preferred x
                    return 0;
                } else {
                    return layout_->offsetInRow(*doc_, ln, row, x);
                }
            } else {
                const auto& l = layout_->line(*doc_, ln);
                if (row + 1 < l.rows.size()) ++row;
                else if (ln + 1 < doc_->lineCount()) { ++ln; row = 0; }
                else break;
            }
        }
        return layout_->offsetInRow(*doc_, ln, row, x);
    }
    }
    return c;
}

void EditorController::replaceSelection(std::u32string_view text, bool coalescable) {
    if (readOnly_) return;
    const auto before = paintState();
    const auto snapBefore = snapshot();
    if (state_.hasSelection()) {
        doc_->breakUndoGroup();
        const std::size_t a = state_.selStart(), b = state_.selEnd();
        auto e = doc_->erase(a, b - a, snapBefore, {a, a});
        const auto rows = layout_->onEdit(*doc_, e);
        state_.caret = state_.anchor = a;
        updates_->onEdit(e, rows, before, paintState());
        coalescable = false;
    } else if (!coalescable) {
        doc_->breakUndoGroup();
    }
    if (!text.empty()) {
        // Normalise line breaks here so the recorded "after" snapshot is exact (the document stores no '\r').
        std::u32string norm;
        norm.reserve(text.size());
        for (std::size_t i = 0; i < text.size(); ++i) {
            if (text[i] == U'\r') {
                norm.push_back(U'\n');
                if (i + 1 < text.size() && text[i + 1] == U'\n') ++i;
            } else {
                norm.push_back(text[i]);
            }
        }
        const std::size_t at = state_.caret;
        const std::size_t end = at + norm.size();
        const auto before2 = paintState();
        auto e = doc_->insert(at, norm, snapBefore, {end, end});
        const auto rows = layout_->onEdit(*doc_, e);
        state_.caret = state_.anchor = end;
        updates_->onEdit(e, rows, before2, paintState());
        if (!coalescable) doc_->breakUndoGroup();
    }
    state_.preferredX.reset();
    viewport_->clampScroll(*layout_);
    ensureCaretVisible();
}

void EditorController::eraseRange(std::size_t from, std::size_t to) {
    if (readOnly_ || from >= to) return;
    const auto before = paintState();
    auto e = doc_->erase(from, to - from, snapshot(), {from, from});
    const auto rows = layout_->onEdit(*doc_, e);
    state_.caret = state_.anchor = from;
    state_.preferredX.reset();
    updates_->onEdit(e, rows, before, paintState());
    viewport_->clampScroll(*layout_);
    ensureCaretVisible();
}

std::size_t EditorController::offsetAtView(int viewX, int viewY) {
    const std::int64_t docY = viewport_->scrollY(*layout_) + viewY;
    return layout_->hitTest(*doc_, viewport_->toDocX(viewX), docY);
}

bool EditorController::handle(const EditorCommand& c) {
    // Scroll changes caused by the command (caret following, clamping after deletions) are reported once,
    // after all edit notifications, so dirty rectangles are shifted into the new scroll position.
    const std::int64_t oldY = viewport_->scrollY(*layout_);
    const int oldX = viewport_->scrollX;
    const auto oldAnchor = viewport_->anchor;
    const bool changed = dispatch(c);
    if (viewport_->scrollX != oldX || viewport_->scrollY(*layout_) != oldY || !(viewport_->anchor == oldAnchor))
        updates_->onScroll(oldY, oldX);
    return changed;
}

bool EditorController::dispatch(const EditorCommand& c) {
    return std::visit(
        Overloaded{
            [&](const cmd::Type& t) {
                if (readOnly_ || t.text.empty()) return false;
                replaceSelection(t.text, t.text.size() == 1 && !state_.hasSelection());
                return true;
            },
            [&](const cmd::Enter&) {
                if (readOnly_) return false;
                replaceSelection(U"\n", true);
                return true;
            },
            [&](const cmd::Backspace&) {
                if (readOnly_) return false;
                if (state_.hasSelection()) { doc_->breakUndoGroup(); eraseRange(state_.selStart(), state_.selEnd()); doc_->breakUndoGroup(); }
                else if (state_.caret > 0) eraseRange(state_.caret - 1, state_.caret);
                else return false;
                return true;
            },
            [&](const cmd::Delete&) {
                if (readOnly_) return false;
                if (state_.hasSelection()) { doc_->breakUndoGroup(); eraseRange(state_.selStart(), state_.selEnd()); doc_->breakUndoGroup(); }
                else if (state_.caret < doc_->length()) eraseRange(state_.caret, state_.caret + 1);
                else return false;
                return true;
            },
            [&](const cmd::Move& m) {
                const bool vertical = m.kind == MoveKind::Up || m.kind == MoveKind::Down ||
                                      m.kind == MoveKind::PageUp || m.kind == MoveKind::PageDown;
                std::size_t target;
                if (!m.extend && state_.hasSelection() && (m.kind == MoveKind::Left || m.kind == MoveKind::Right))
                    target = moveTarget(m.kind);
                else {
                    // Moves are computed from the caret; collapse handling for Left/Right is above.
                    const std::size_t savedAnchor = state_.anchor;
                    state_.anchor = state_.caret;
                    target = moveTarget(m.kind);
                    state_.anchor = savedAnchor;
                }
                moveCaret(target, m.extend, vertical);
                return true;
            },
            [&](const cmd::SelectAll&) {
                const auto before = paintState();
                state_.anchor = 0;
                state_.caret = doc_->length();
                updates_->onCaretMoved(before, paintState());
                ensureCaretVisible();
                return true;
            },
            [&](const cmd::Undo&) {
                if (readOnly_) return false;
                const auto before = paintState();
                document::EditorSnapshot snap;
                std::vector<document::EditResult> edits;
                if (!doc_->undo(&snap, &edits)) return false;
                state_.caret = std::min(snap.caret, doc_->length());
                state_.anchor = std::min(snap.anchor, doc_->length());
                for (const auto& e : edits) updates_->onEdit(e, layout_->onEdit(*doc_, e), before, paintState());
                viewport_->clampScroll(*layout_);
                ensureCaretVisible();
                return true;
            },
            [&](const cmd::Redo&) {
                if (readOnly_) return false;
                const auto before = paintState();
                document::EditorSnapshot snap;
                std::vector<document::EditResult> edits;
                if (!doc_->redo(&snap, &edits)) return false;
                state_.caret = std::min(snap.caret, doc_->length());
                state_.anchor = std::min(snap.anchor, doc_->length());
                for (const auto& e : edits) updates_->onEdit(e, layout_->onEdit(*doc_, e), before, paintState());
                viewport_->clampScroll(*layout_);
                ensureCaretVisible();
                return true;
            },
            [&](const cmd::Copy&) {
                if (!state_.hasSelection()) return false;
                return clipboard_->setText(doc_->text(state_.selStart(), state_.selEnd() - state_.selStart()));
            },
            [&](const cmd::Cut&) {
                if (readOnly_ || !state_.hasSelection()) return false;
                if (!clipboard_->setText(doc_->text(state_.selStart(), state_.selEnd() - state_.selStart()))) return false;
                doc_->breakUndoGroup();
                eraseRange(state_.selStart(), state_.selEnd());
                doc_->breakUndoGroup();
                return true;
            },
            [&](const cmd::Paste&) {
                if (readOnly_) return false;
                auto t = clipboard_->getText();
                if (!t || t->empty()) return false;
                replaceSelection(*t, false);
                return true;
            },
            [&](const cmd::ToggleWrap&) {
                state_.wordWrap = !state_.wordWrap;
                applyLayoutParams(scale_);
                updates_->onWrapToggled();
                ensureCaretVisible();
                return true;
            },
            [&](const cmd::SetFont& f) {
                if (f.packId == state_.activePackId) return false;
                state_.activePackId = f.packId;
                applyLayoutParams(scale_);
                updates_->onFontChanged();
                ensureCaretVisible();
                return true;
            },
            [&](const cmd::Click& k) {
                moveCaret(offsetAtView(k.viewX, k.viewY), k.extend);
                return true;
            },
            [&](const cmd::Drag& d) {
                moveCaret(offsetAtView(d.viewX, d.viewY), true);
                return true;
            },
            [&](const cmd::SelectWordAt& w) {
                const std::size_t at = offsetAtView(w.viewX, w.viewY);
                std::size_t a = at, b = at;
                while (a > 0 && isWordChar(doc_->at(a - 1))) --a;
                while (b < doc_->length() && isWordChar(doc_->at(b))) ++b;
                const auto before = paintState();
                state_.anchor = a;
                state_.caret = b;
                updates_->onCaretMoved(before, paintState());
                return true;
            },
        },
        c);
}

} // namespace pxc::editor
