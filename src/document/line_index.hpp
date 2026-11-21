#pragma once
// Line index: per-line lengths (each including its '\n', the last line has none) kept in a Fenwick
// tree for O(log n) offset <-> line mapping, updated incrementally on edits.

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "pxc/core/fenwick.hpp"

namespace pxc::document {

class LineIndex {
public:
    void rebuild(std::u32string_view text);
    std::size_t lineCount() const noexcept { return lengths_.size(); }
    std::size_t lineStart(std::size_t line) const { return static_cast<std::size_t>(tree_.prefix(line)); }
    std::size_t lineLengthWithBreak(std::size_t line) const { return static_cast<std::size_t>(lengths_[line]); }
    std::size_t lineOfOffset(std::size_t offset) const;
    // Replaces lines [first, first+oldCount) with lines of the given lengths (each including its '\n'
    // except possibly the last line of the document).
    void replace(std::size_t first, std::size_t oldCount, const std::vector<std::int64_t>& newLengths);

private:
    std::vector<std::int64_t> lengths_{0};
    core::Fenwick tree_{lengths_};
};

} // namespace pxc::document
