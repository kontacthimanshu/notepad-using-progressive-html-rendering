#pragma once
// Per-frame instrumentation (research R-16, FR-023).

#include <cstdint>

namespace pxc::update {

struct RenderStats {
    std::uint64_t frame = 0;
    std::uint64_t cacheHits = 0;
    std::uint64_t cacheMisses = 0;
    double cacheHitRate = 1.0;
    std::uint64_t rasterizations = 0;
    std::uint64_t linesLaidOut = 0;
    std::uint64_t rowsRecomputed = 0;
    std::uint64_t glyphBlits = 0;
    std::uint64_t dirtyRectCount = 0;
    std::uint64_t dirtyPixelArea = 0;
    bool fullRepaint = false;
    int scrollShift = 0; // device px the existing pixels were moved by (scrolling)
    std::int64_t layoutMicros = 0;
    std::int64_t paintMicros = 0;
    std::int64_t keyToPresentMicros = -1; // filled in by the shell after presentation
};

struct IRenderObserver {
    virtual void onFrame(const RenderStats& stats) = 0;
    virtual ~IRenderObserver() = default;
};

} // namespace pxc::update
