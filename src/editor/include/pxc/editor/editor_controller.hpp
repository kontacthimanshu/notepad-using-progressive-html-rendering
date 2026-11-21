#pragma once
// Editor controller: keyboard/mouse editing commands over TextDocument, caret and selection
// (data-model §11). Font switches and wrap toggles are presentation-only and never touch the text.

#include <optional>
#include <string>
#include <variant>

#include "pxc/document/text_document.hpp"
#include "pxc/editor/clipboard.hpp"
#include "pxc/layout/layout_engine.hpp"
#include "pxc/layout/viewport.hpp"
#include "pxc/update/update_manager.hpp"

namespace pxc::editor {

enum class EditorMode { EditPlainText, PreviewHtml };

struct EditorState {
    std::size_t caret = 0;
    std::size_t anchor = 0;
    std::optional<int> preferredX; // kept across vertical moves; recomputed after a font change
    bool wordWrap = true;
    std::string activePackId = std::string(fonts::kDefaultPackId);
    EditorMode mode = EditorMode::EditPlainText;

    std::size_t selStart() const noexcept { return caret < anchor ? caret : anchor; }
    std::size_t selEnd() const noexcept { return caret < anchor ? anchor : caret; }
    bool hasSelection() const noexcept { return caret != anchor; }
};

enum class MoveKind { Left, Right, Up, Down, Home, End, DocStart, DocEnd, WordLeft, WordRight, PageUp, PageDown };

namespace cmd {
struct Type { std::u32string text; };
struct Enter {};
struct Backspace {};
struct Delete {};
struct Move { MoveKind kind; bool extend = false; };
struct SelectAll {};
struct Undo {};
struct Redo {};
struct Copy {};
struct Cut {};
struct Paste {};
struct ToggleWrap {};
struct SetFont { std::string packId; };
struct Click { int viewX = 0; int viewY = 0; bool extend = false; };
struct Drag { int viewX = 0; int viewY = 0; };
struct SelectWordAt { int viewX = 0; int viewY = 0; };
} // namespace cmd

using EditorCommand = std::variant<cmd::Type, cmd::Enter, cmd::Backspace, cmd::Delete, cmd::Move, cmd::SelectAll,
                                   cmd::Undo, cmd::Redo, cmd::Copy, cmd::Cut, cmd::Paste, cmd::ToggleWrap,
                                   cmd::SetFont, cmd::Click, cmd::Drag, cmd::SelectWordAt>;

class EditorController {
public:
    EditorController(document::TextDocument& doc, layout::LayoutEngine& layout, layout::Viewport& viewport,
                     update::UpdateManager& updates, IClipboard& clipboard);

    // Returns true when the command changed the text, caret, selection or presentation.
    bool handle(const EditorCommand& c);

    const EditorState& state() const noexcept { return state_; }
    update::PaintState paintState() const;

    // Replaces the document text (New/Open). Resets caret, selection and scroll.
    void loadText(std::u32string_view text);
    // Applies the current wrap/pack/scale to the layout engine (call after resize or DPI change).
    void applyLayoutParams(int scale);
    void setReadOnly(bool ro) noexcept { readOnly_ = ro; }
    bool readOnly() const noexcept { return readOnly_; }
    void setMode(EditorMode m) noexcept { state_.mode = m; }
    void ensureCaretVisible();

    document::TextDocument& document() noexcept { return *doc_; }

private:
    bool dispatch(const EditorCommand& c);
    void replaceSelection(std::u32string_view text, bool coalescable);
    void eraseRange(std::size_t from, std::size_t to);
    void moveCaret(std::size_t newCaret, bool extend, bool keepPreferredX = false);
    std::size_t moveTarget(MoveKind k);
    std::size_t wordBoundary(std::size_t from, int dir) const;
    std::size_t offsetAtView(int viewX, int viewY);
    document::EditorSnapshot snapshot() const { return {state_.caret, state_.anchor}; }

    document::TextDocument* doc_;
    layout::LayoutEngine* layout_;
    layout::Viewport* viewport_;
    update::UpdateManager* updates_;
    IClipboard* clipboard_;
    EditorState state_;
    int scale_ = 1;
    bool readOnly_ = false;
};

} // namespace pxc::editor
