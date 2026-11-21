#pragma once
// Logical Unicode text model (data-model §6): a piece table of char32_t code points with an incremental
// line index, per-line revisions and coalescing undo/redo. Never stores glyph pixels (FR-016).

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "pxc/core/clock.hpp"

namespace pxc::document {

struct LineCol {
    std::size_t line = 0;
    std::size_t col = 0;
    bool operator==(const LineCol&) const = default;
};

// Caret/selection state captured with each edit so undo/redo can restore it.
struct EditorSnapshot {
    std::size_t caret = 0;
    std::size_t anchor = 0;
    bool operator==(const EditorSnapshot&) const = default;
};

struct EditResult {
    bool valid = false;
    std::size_t offset = 0;
    std::size_t removed = 0;
    std::size_t inserted = 0;
    std::size_t firstLine = 0;    // first logical line touched
    std::size_t oldLineCount = 0; // lines [firstLine, firstLine+oldLineCount) before the edit ...
    std::size_t newLineCount = 0; // ... became [firstLine, firstLine+newLineCount) after it
    std::uint64_t oldFirstLineRevision = 0; // revision of firstLine before the edit (for incremental relayout)
    std::size_t firstLineColumn = 0;        // edit offset relative to the start of firstLine
};

inline constexpr std::size_t kMaxUndoGroups = 10000;

class TextDocument {
public:
    explicit TextDocument(const core::IClock* clock = nullptr);
    ~TextDocument();
    TextDocument(const TextDocument&) = delete;
    TextDocument& operator=(const TextDocument&) = delete;

    // Replaces the whole text (file load / New). Clears undo history and the modified flag.
    void setText(std::u32string_view text);

    std::size_t length() const noexcept;
    std::size_t lineCount() const noexcept;
    std::size_t lineStart(std::size_t line) const;
    std::size_t lineLength(std::size_t line) const; // excluding the '\n'
    std::size_t lineOfOffset(std::size_t offset) const;
    LineCol toLineCol(std::size_t offset) const;
    std::size_t toOffset(LineCol lc) const; // column is clamped to the line length

    char32_t at(std::size_t offset) const;
    std::u32string text(std::size_t offset, std::size_t count) const;
    std::u32string text() const;
    std::u32string lineText(std::size_t line) const;

    // Mutations. '\r' is never stored: "\r\n" becomes "\n" and a lone '\r' becomes '\n'.
    EditResult insert(std::size_t offset, std::u32string_view text, EditorSnapshot before = {},
                      EditorSnapshot after = {});
    EditResult erase(std::size_t offset, std::size_t count, EditorSnapshot before = {}, EditorSnapshot after = {});

    // Undo/redo one coalesced group. `snap` receives the caret/selection to restore; `edits` the applied changes.
    bool undo(EditorSnapshot* snap = nullptr, std::vector<EditResult>* edits = nullptr);
    bool redo(EditorSnapshot* snap = nullptr, std::vector<EditResult>* edits = nullptr);
    bool canUndo() const noexcept;
    bool canRedo() const noexcept;
    std::size_t undoGroupCount() const noexcept;
    // Forces the next edit to start a new undo group (e.g. after a caret move).
    void breakUndoGroup() noexcept;

    std::uint64_t revision() const noexcept;
    std::uint64_t lineRevision(std::size_t line) const;

    bool modified() const noexcept;
    void markSaved() noexcept;
    void markModified() noexcept; // e.g. after opening with replacement characters

    struct Impl;

private:
    std::unique_ptr<Impl> impl_;
};

} // namespace pxc::document
