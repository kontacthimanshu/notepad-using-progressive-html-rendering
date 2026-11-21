#pragma once
// Plain cached pixel representation of a glyph. No knowledge of glyph CSS (data-model §5).

#include <cstdint>
#include <vector>

namespace pxc::render {

struct RasterGlyph {
    int width = 0;   // pixels (cells * scale)
    int height = 0;  // pixels
    int originX = 0; // offset from pen x to the left edge  (= bearingX * scale)
    int originY = 0; // offset from baseline to the top edge (= -baseline * scale)
    std::vector<std::uint32_t> pixels; // straight-alpha BGRA, row-major, width*height

    std::size_t byteSize() const noexcept { return pixels.size() * sizeof(std::uint32_t) + sizeof(*this); }
};

} // namespace pxc::render
