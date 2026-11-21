#include "piece_table.hpp"

#include <stdexcept>

namespace pxc::document {

void PieceTable::reset(std::u32string_view original) {
    original_.assign(original);
    add_.clear();
    pieces_.clear();
    if (!original_.empty()) pieces_.push_back({false, 0, original_.size()});
    length_ = original_.size();
    cachePiece_ = cacheStart_ = 0;
}

std::size_t PieceTable::locate(std::size_t offset, std::size_t& within) const {
    std::size_t i = 0, start = 0;
    if (cachePiece_ < pieces_.size() && cacheStart_ <= offset) {
        i = cachePiece_;
        start = cacheStart_;
    }
    for (; i < pieces_.size(); ++i) {
        if (offset < start + pieces_[i].len) {
            cachePiece_ = i;
            cacheStart_ = start;
            within = offset - start;
            return i;
        }
        start += pieces_[i].len;
    }
    within = 0;
    return pieces_.size();
}

void PieceTable::insert(std::size_t offset, std::u32string_view text) {
    if (text.empty()) return;
    if (offset > length_) offset = length_;
    const std::size_t addStart = add_.size();
    add_.append(text);
    cachePiece_ = cacheStart_ = 0;

    std::size_t within = 0;
    std::size_t i = offset == length_ ? pieces_.size() : locate(offset, within);
    if (i == pieces_.size()) {
        // Append at the end: merge with the last piece when it ends exactly at addStart.
        if (!pieces_.empty() && pieces_.back().add && pieces_.back().start + pieces_.back().len == addStart) {
            pieces_.back().len += text.size();
        } else {
            pieces_.push_back({true, addStart, text.size()});
        }
    } else if (within == 0) {
        // Insert before piece i; merge with the previous piece when contiguous in the add buffer.
        if (i > 0 && pieces_[i - 1].add && pieces_[i - 1].start + pieces_[i - 1].len == addStart) {
            pieces_[i - 1].len += text.size();
        } else {
            pieces_.insert(pieces_.begin() + static_cast<std::ptrdiff_t>(i), Piece{true, addStart, text.size()});
        }
    } else {
        // Split piece i.
        Piece left = pieces_[i];
        Piece right = pieces_[i];
        left.len = within;
        right.start += within;
        right.len -= within;
        pieces_[i] = left;
        pieces_.insert(pieces_.begin() + static_cast<std::ptrdiff_t>(i) + 1, Piece{true, addStart, text.size()});
        pieces_.insert(pieces_.begin() + static_cast<std::ptrdiff_t>(i) + 2, right);
    }
    length_ += text.size();
    cachePiece_ = cacheStart_ = 0; // pieces changed: invalidate the locate cache
}

void PieceTable::erase(std::size_t offset, std::size_t count) {
    if (offset >= length_ || count == 0) return;
    if (count > length_ - offset) count = length_ - offset;
    cachePiece_ = cacheStart_ = 0;
    std::size_t within = 0;
    std::size_t i = locate(offset, within);
    cachePiece_ = cacheStart_ = 0;
    std::size_t remaining = count;
    if (within > 0) {
        // Split so the erase starts at a piece boundary.
        Piece right = pieces_[i];
        right.start += within;
        right.len -= within;
        pieces_[i].len = within;
        pieces_.insert(pieces_.begin() + static_cast<std::ptrdiff_t>(i) + 1, right);
        ++i;
    }
    while (remaining > 0 && i < pieces_.size()) {
        if (pieces_[i].len <= remaining) {
            remaining -= pieces_[i].len;
            pieces_.erase(pieces_.begin() + static_cast<std::ptrdiff_t>(i));
        } else {
            pieces_[i].start += remaining;
            pieces_[i].len -= remaining;
            remaining = 0;
        }
    }
    length_ -= count;
    cachePiece_ = cacheStart_ = 0;
}

char32_t PieceTable::at(std::size_t offset) const {
    if (offset >= length_) throw std::out_of_range("PieceTable::at");
    std::size_t within = 0;
    const std::size_t i = locate(offset, within);
    return data(pieces_[i])[within];
}

std::u32string PieceTable::text(std::size_t offset, std::size_t count) const {
    std::u32string out;
    if (offset >= length_) return out;
    if (count > length_ - offset) count = length_ - offset;
    out.reserve(count);
    forEachChunk(offset, count, [&](const char32_t* p, std::size_t n) { out.append(p, n); });
    return out;
}

} // namespace pxc::document
