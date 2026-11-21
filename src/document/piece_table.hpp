#pragma once
// Piece table over char32_t code points (research R-10): an immutable original buffer, an append-only
// add buffer and an ordered vector of pieces. Adjacent appends to the add buffer merge into one piece.

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace pxc::document {

class PieceTable {
public:
    void reset(std::u32string_view original);
    std::size_t length() const noexcept { return length_; }
    std::size_t pieceCount() const noexcept { return pieces_.size(); }

    void insert(std::size_t offset, std::u32string_view text);
    void erase(std::size_t offset, std::size_t count);
    char32_t at(std::size_t offset) const;
    std::u32string text(std::size_t offset, std::size_t count) const;
    // Calls f(ptr, len) for each contiguous chunk of [offset, offset+count).
    template <class F> void forEachChunk(std::size_t offset, std::size_t count, F&& f) const;

private:
    struct Piece {
        bool add = false;
        std::size_t start = 0;
        std::size_t len = 0;
    };
    const char32_t* data(const Piece& p) const noexcept { return (p.add ? add_.data() : original_.data()) + p.start; }
    // Index of the piece containing `offset` and the offset within it (offset == length -> one past last).
    std::size_t locate(std::size_t offset, std::size_t& within) const;

    std::u32string original_;
    std::u32string add_;
    std::vector<Piece> pieces_;
    std::size_t length_ = 0;
    // Cache of the last located piece (speeds up sequential access).
    mutable std::size_t cachePiece_ = 0;
    mutable std::size_t cacheStart_ = 0;
};

template <class F> void PieceTable::forEachChunk(std::size_t offset, std::size_t count, F&& f) const {
    if (count == 0 || offset >= length_) return;
    std::size_t within = 0;
    std::size_t i = locate(offset, within);
    while (count > 0 && i < pieces_.size()) {
        const Piece& p = pieces_[i];
        const std::size_t n = (p.len - within) < count ? (p.len - within) : count;
        f(data(p) + within, n);
        count -= n;
        within = 0;
        ++i;
    }
}

} // namespace pxc::document
