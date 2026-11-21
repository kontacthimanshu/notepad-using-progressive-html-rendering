#include "line_index.hpp"

namespace pxc::document {

void LineIndex::rebuild(std::u32string_view text) {
    lengths_.clear();
    std::int64_t cur = 0;
    for (char32_t c : text) {
        ++cur;
        if (c == U'\n') {
            lengths_.push_back(cur);
            cur = 0;
        }
    }
    lengths_.push_back(cur);
    tree_.assign(lengths_);
}

std::size_t LineIndex::lineOfOffset(std::size_t offset) const {
    // Number of lines whose cumulative length is <= offset, i.e. the line containing `offset`.
    std::size_t line = tree_.upperIndex(static_cast<std::int64_t>(offset));
    if (line >= lengths_.size()) line = lengths_.size() - 1;
    return line;
}

void LineIndex::replace(std::size_t first, std::size_t oldCount, const std::vector<std::int64_t>& newLengths) {
    if (oldCount == newLengths.size()) {
        for (std::size_t k = 0; k < oldCount; ++k) {
            const std::int64_t d = newLengths[k] - lengths_[first + k];
            if (d != 0) {
                lengths_[first + k] = newLengths[k];
                tree_.add(first + k, d);
            }
        }
        return;
    }
    lengths_.erase(lengths_.begin() + static_cast<std::ptrdiff_t>(first),
                   lengths_.begin() + static_cast<std::ptrdiff_t>(first + oldCount));
    lengths_.insert(lengths_.begin() + static_cast<std::ptrdiff_t>(first), newLengths.begin(), newLengths.end());
    if (lengths_.empty()) lengths_.push_back(0);
    tree_.assign(lengths_);
}

} // namespace pxc::document
