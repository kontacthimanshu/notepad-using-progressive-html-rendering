#include "pxc/update/dirty_region.hpp"

namespace pxc::update {

void DirtyRegion::add(core::Rect r) {
    if (full_) return;
    r = r.intersect(bounds_);
    if (r.empty()) return;
    // Merge with an overlapping or touching rectangle (repeat until stable).
    bool merged = true;
    while (merged) {
        merged = false;
        for (std::size_t i = 0; i < count_; ++i) {
            const core::Rect& o = rects_[i];
            const core::Rect grown{o.x - 1, o.y - 1, o.w + 2, o.h + 2};
            if (grown.intersects(r)) {
                r = r.unite(o);
                rects_[i] = rects_[count_ - 1];
                --count_;
                merged = true;
                break;
            }
        }
    }
    if (count_ == kMaxDirtyRects) {
        core::Rect box = r;
        for (std::size_t i = 0; i < count_; ++i) box = box.unite(rects_[i]);
        rects_[0] = box;
        count_ = 1;
        return;
    }
    rects_[count_++] = r;
}

void DirtyRegion::clear() noexcept {
    count_ = 0;
    full_ = false;
}

void DirtyRegion::shiftY(int dy) noexcept {
    if (full_ || dy == 0) return;
    std::size_t out = 0;
    for (std::size_t i = 0; i < count_; ++i) {
        core::Rect r = rects_[i];
        r.y += dy;
        r = r.intersect(bounds_);
        if (!r.empty()) rects_[out++] = r;
    }
    count_ = out;
}

core::Rect DirtyRegion::boundingBox() const noexcept {
    if (full_) return bounds_;
    core::Rect box{};
    for (std::size_t i = 0; i < count_; ++i) box = box.unite(rects_[i]);
    return box;
}

std::int64_t DirtyRegion::area() const noexcept {
    if (full_) return bounds_.area();
    std::int64_t a = 0;
    for (std::size_t i = 0; i < count_; ++i) a += rects_[i].area();
    return a;
}

} // namespace pxc::update
