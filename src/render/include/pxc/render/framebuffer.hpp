#pragma once
// Internally owned software raster surface: 32-bit straight-alpha BGRA, top-down rows (research R-09).
// pxc_render depends only on pxc_core: it knows pixels and rectangles, never glyph CSS or text.

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "pxc/core/expected.hpp"
#include "pxc/core/geometry.hpp"

namespace pxc::render {

using core::Color;
using core::Rect;

inline constexpr int kMaxFramebufferDim = 16384;

class Framebuffer {
public:
    Framebuffer() = default;
    // Creates a w×h surface filled with `fill`. Dimensions beyond kMaxFramebufferDim produce an empty surface;
    // use create() to get the error.
    Framebuffer(int w, int h, Color fill = core::kTransparent);

    static core::Expected<Framebuffer, std::string> create(int w, int h, Color fill = core::kTransparent);

    // Resizes (contents are reset to `fill`). Fails for negative or oversized dimensions.
    core::Expected<void, std::string> resize(int w, int h, Color fill = core::kTransparent);

    int width() const noexcept { return w_; }
    int height() const noexcept { return h_; }
    Rect bounds() const noexcept { return {0, 0, w_, h_}; }
    bool empty() const noexcept { return w_ == 0 || h_ == 0; }

    std::span<std::uint32_t> row(int y) noexcept {
        return {px_.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(w_), static_cast<std::size_t>(w_)};
    }
    std::span<const std::uint32_t> row(int y) const noexcept {
        return {px_.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(w_), static_cast<std::size_t>(w_)};
    }
    std::uint32_t pixel(int x, int y) const noexcept {
        return px_[static_cast<std::size_t>(y) * static_cast<std::size_t>(w_) + static_cast<std::size_t>(x)];
    }
    Color color(int x, int y) const noexcept { return Color::fromBGRA(pixel(x, y)); }
    void setPixel(int x, int y, std::uint32_t v) noexcept {
        px_[static_cast<std::size_t>(y) * static_cast<std::size_t>(w_) + static_cast<std::size_t>(x)] = v;
    }

    // Fills `r` clipped to the surface (opaque write, no blending).
    void fillRect(Rect r, Color c) noexcept;

    std::span<const std::uint32_t> pixels() const noexcept { return px_; }
    std::span<std::uint32_t> pixels() noexcept { return px_; }

    bool operator==(const Framebuffer& o) const noexcept { return w_ == o.w_ && h_ == o.h_ && px_ == o.px_; }

private:
    int w_ = 0, h_ = 0;
    std::vector<std::uint32_t> px_;
};

} // namespace pxc::render
