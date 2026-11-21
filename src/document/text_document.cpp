#include "pxc/document/text_document.hpp"

#include <algorithm>
#include <chrono>
#include <deque>

#include "line_index.hpp"
#include "piece_table.hpp"

namespace pxc::document {

namespace {

struct Record {
    std::size_t offset = 0;
    std::u32string removed;
    std::u32string inserted;
    EditorSnapshot before, after;
    std::chrono::microseconds time{0};
};

struct Group {
    std::vector<Record> records;
    bool sealed = false;
};

bool isSpace(char32_t c) { return c == U' ' || c == U'\t' || c == U'\n'; }

std::u32string normalizeNewlines(std::u32string_view in) {
    std::u32string out;
    out.reserve(in.size());
    for (std::size_t i = 0; i < in.size(); ++i) {
        if (in[i] == U'\r') {
            out.push_back(U'\n');
            if (i + 1 < in.size() && in[i + 1] == U'\n') ++i;
        } else {
            out.push_back(in[i]);
        }
    }
    return out;
}

} // namespace

struct TextDocument::Impl {
    const core::IClock* clock = nullptr;
    core::SteadyClock steady;
    PieceTable pt;
    LineIndex li;
    std::vector<std::uint64_t> lineRev{1};
    std::uint64_t revCounter = 1;
    std::uint64_t revision = 1;
    std::deque<Group> undo;
    std::vector<Group> redo;
    long long savedIndex = 0; // undo.size() at the last save; -1 = unreachable
    bool forcedModified = false;

    std::chrono::microseconds now() const { return clock ? clock->now() : steady.now(); }

    EditResult apply(std::size_t offset, std::size_t removeCount, std::u32string_view text) {
        EditResult r;
        const std::size_t len = pt.length();
        if (offset > len) offset = len;
        if (removeCount > len - offset) removeCount = len - offset;
        const std::size_t firstLine = li.lineOfOffset(offset);
        const std::size_t lastLine = li.lineOfOffset(offset + removeCount);
        const std::size_t regionStart = li.lineStart(firstLine);
        const std::size_t regionOldLen = li.lineStart(lastLine) + li.lineLengthWithBreak(lastLine) - regionStart;
        const bool regionIsTail = lastLine + 1 == li.lineCount();
        r.oldFirstLineRevision = lineRev[firstLine];
        r.firstLineColumn = offset - regionStart;

        pt.erase(offset, removeCount);
        pt.insert(offset, text);

        const std::size_t newRegionLen = regionOldLen - removeCount + text.size();
        std::vector<std::int64_t> lengths;
        std::int64_t cur = 0;
        pt.forEachChunk(regionStart, newRegionLen, [&](const char32_t* p, std::size_t n) {
            for (std::size_t k = 0; k < n; ++k) {
                ++cur;
                if (p[k] == U'\n') {
                    lengths.push_back(cur);
                    cur = 0;
                }
            }
        });
        if (regionIsTail || cur > 0 || lengths.empty()) lengths.push_back(cur);
        const std::size_t oldCount = lastLine - firstLine + 1;
        li.replace(firstLine, oldCount, lengths);
        lineRev.erase(lineRev.begin() + static_cast<std::ptrdiff_t>(firstLine),
                      lineRev.begin() + static_cast<std::ptrdiff_t>(firstLine + oldCount));
        std::vector<std::uint64_t> fresh(lengths.size());
        for (auto& v : fresh) v = ++revCounter;
        lineRev.insert(lineRev.begin() + static_cast<std::ptrdiff_t>(firstLine), fresh.begin(), fresh.end());
        ++revision;

        r.valid = true;
        r.offset = offset;
        r.removed = removeCount;
        r.inserted = text.size();
        r.firstLine = firstLine;
        r.oldLineCount = oldCount;
        r.newLineCount = lengths.size();
        return r;
    }

    void pushRecord(Record rec) {
        redo.clear();
        Group* top = undo.empty() ? nullptr : &undo.back();
        bool coalesce = false;
        if (top && !top->sealed && !top->records.empty()) {
            const Record& last = top->records.back();
            const bool recent = rec.time - last.time <= std::chrono::seconds(1);
            const bool lastIsCharInsert = last.removed.empty() && last.inserted.size() == 1;
            const bool newIsCharInsert = rec.removed.empty() && rec.inserted.size() == 1;
            const bool lastIsCharErase = last.inserted.empty() && last.removed.size() == 1;
            const bool newIsCharErase = rec.inserted.empty() && rec.removed.size() == 1;
            if (recent && lastIsCharInsert && newIsCharInsert && rec.offset == last.offset + 1 &&
                !(isSpace(last.inserted[0]) && !isSpace(rec.inserted[0]))) {
                coalesce = true;
            } else if (recent && lastIsCharErase && newIsCharErase &&
                       (rec.offset + 1 == last.offset || rec.offset == last.offset)) {
                coalesce = true;
            }
            // Coalescing into the group that defines the saved state would hide the change.
            if (coalesce && savedIndex == static_cast<long long>(undo.size())) coalesce = false;
        }
        if (coalesce) {
            top->records.push_back(std::move(rec));
            return;
        }
        if (savedIndex > static_cast<long long>(undo.size())) savedIndex = -1;
        Group g;
        g.records.push_back(std::move(rec));
        undo.push_back(std::move(g));
        if (undo.size() > kMaxUndoGroups) {
            undo.pop_front();
            if (savedIndex >= 0) savedIndex = savedIndex == 0 ? -1 : savedIndex - 1;
        }
    }
};

TextDocument::TextDocument(const core::IClock* clock) : impl_(std::make_unique<Impl>()) {
    impl_->clock = clock;
    impl_->li.rebuild(U"");
}
TextDocument::~TextDocument() = default;

void TextDocument::setText(std::u32string_view text) {
    const std::u32string norm = normalizeNewlines(text);
    impl_->pt.reset(norm);
    impl_->li.rebuild(norm);
    impl_->lineRev.assign(impl_->li.lineCount(), 0);
    for (auto& v : impl_->lineRev) v = ++impl_->revCounter;
    ++impl_->revision;
    impl_->undo.clear();
    impl_->redo.clear();
    impl_->savedIndex = 0;
    impl_->forcedModified = false;
}

std::size_t TextDocument::length() const noexcept { return impl_->pt.length(); }
std::size_t TextDocument::lineCount() const noexcept { return impl_->li.lineCount(); }
std::size_t TextDocument::lineStart(std::size_t line) const {
    return impl_->li.lineStart(std::min(line, lineCount() - 1));
}
std::size_t TextDocument::lineLength(std::size_t line) const {
    line = std::min(line, lineCount() - 1);
    std::size_t n = impl_->li.lineLengthWithBreak(line);
    if (line + 1 < lineCount() && n > 0) --n; // exclude '\n'
    return n;
}
std::size_t TextDocument::lineOfOffset(std::size_t offset) const {
    return impl_->li.lineOfOffset(std::min(offset, length()));
}
LineCol TextDocument::toLineCol(std::size_t offset) const {
    offset = std::min(offset, length());
    const std::size_t line = lineOfOffset(offset);
    return {line, offset - lineStart(line)};
}
std::size_t TextDocument::toOffset(LineCol lc) const {
    const std::size_t line = std::min(lc.line, lineCount() - 1);
    return lineStart(line) + std::min(lc.col, lineLength(line));
}
char32_t TextDocument::at(std::size_t offset) const { return impl_->pt.at(offset); }
std::u32string TextDocument::text(std::size_t offset, std::size_t count) const { return impl_->pt.text(offset, count); }
std::u32string TextDocument::text() const { return impl_->pt.text(0, length()); }
std::u32string TextDocument::lineText(std::size_t line) const { return text(lineStart(line), lineLength(line)); }

EditResult TextDocument::insert(std::size_t offset, std::u32string_view text, EditorSnapshot before,
                                EditorSnapshot after) {
    const std::u32string norm = normalizeNewlines(text);
    if (norm.empty()) return {};
    offset = std::min(offset, length());
    EditResult r = impl_->apply(offset, 0, norm);
    Record rec;
    rec.offset = offset;
    rec.inserted = norm;
    rec.before = before;
    rec.after = after;
    rec.time = impl_->now();
    impl_->pushRecord(std::move(rec));
    return r;
}

EditResult TextDocument::erase(std::size_t offset, std::size_t count, EditorSnapshot before, EditorSnapshot after) {
    if (offset >= length() || count == 0) return {};
    count = std::min(count, length() - offset);
    Record rec;
    rec.offset = offset;
    rec.removed = impl_->pt.text(offset, count);
    rec.before = before;
    rec.after = after;
    rec.time = impl_->now();
    EditResult r = impl_->apply(offset, count, {});
    impl_->pushRecord(std::move(rec));
    return r;
}

bool TextDocument::undo(EditorSnapshot* snap, std::vector<EditResult>* edits) {
    if (impl_->undo.empty()) return false;
    Group g = std::move(impl_->undo.back());
    impl_->undo.pop_back();
    for (auto it = g.records.rbegin(); it != g.records.rend(); ++it) {
        EditResult r = impl_->apply(it->offset, it->inserted.size(), it->removed);
        if (edits) edits->push_back(r);
    }
    if (snap) *snap = g.records.front().before;
    g.sealed = true;
    impl_->redo.push_back(std::move(g));
    return true;
}

bool TextDocument::redo(EditorSnapshot* snap, std::vector<EditResult>* edits) {
    if (impl_->redo.empty()) return false;
    Group g = std::move(impl_->redo.back());
    impl_->redo.pop_back();
    for (const auto& rec : g.records) {
        EditResult r = impl_->apply(rec.offset, rec.removed.size(), rec.inserted);
        if (edits) edits->push_back(r);
    }
    if (snap) *snap = g.records.back().after;
    g.sealed = true;
    impl_->undo.push_back(std::move(g));
    return true;
}

bool TextDocument::canUndo() const noexcept { return !impl_->undo.empty(); }
bool TextDocument::canRedo() const noexcept { return !impl_->redo.empty(); }
std::size_t TextDocument::undoGroupCount() const noexcept { return impl_->undo.size(); }
void TextDocument::breakUndoGroup() noexcept {
    if (!impl_->undo.empty()) impl_->undo.back().sealed = true;
}

std::uint64_t TextDocument::revision() const noexcept { return impl_->revision; }
std::uint64_t TextDocument::lineRevision(std::size_t line) const {
    return impl_->lineRev[std::min(line, impl_->lineRev.size() - 1)];
}

bool TextDocument::modified() const noexcept {
    return impl_->forcedModified || impl_->savedIndex != static_cast<long long>(impl_->undo.size());
}
void TextDocument::markSaved() noexcept {
    impl_->savedIndex = static_cast<long long>(impl_->undo.size());
    impl_->forcedModified = false;
    breakUndoGroup();
}
void TextDocument::markModified() noexcept { impl_->forcedModified = true; }

} // namespace pxc::document
