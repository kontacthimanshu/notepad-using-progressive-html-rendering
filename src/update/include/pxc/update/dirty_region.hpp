#pragma once
// Portion of the framebuffer requiring repaint (data-model §10).

#include <array>
#include <cstddef>
#include <span>

#include "pxc/core/geometry.hpp"

namespace pxc::update {

inline constexpr std::size_t kMaxDirtyRects = 16;

class DirtyRegion {
public:
    // Adds a rectangle (clipped to `bounds` when set). Overlapping rectangles are merged; adding beyond
    // kMaxDirtyRects collapses the region into its bounding box.
    void add(core::Rect r);
    void markFull() noexcept { full_ = true; }
    void setBounds(core::Rect b) noexcept { bounds_ = b; }
    void clear() noexcept;
    // Moves every rectangle vertically (scrolling); rectangles leaving the bounds are clipped or dropped.
    void shiftY(int dy) noexcept;

    bool full() const noexcept { return full_; }
    bool empty() const noexcept { return !full_ && count_ == 0; }
    std::span<const core::Rect> rects() const noexcept { return {rects_.data(), count_}; }
    core::Rect boundingBox() const noexcept;
    std::int64_t area() const noexcept;
    const core::Rect& bounds() const noexcept { return bounds_; }

private:
    std::array<core::Rect, kMaxDirtyRects> rects_{};
    std::size_t count_ = 0;
    bool full_ = false;
    core::Rect bounds_{0, 0, 1 << 30, 1 << 30};
};

} // namespace pxc::update
