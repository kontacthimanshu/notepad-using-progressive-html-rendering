#pragma once
// Integer rectangles and RGBA colours. All document coordinates are integer device pixels.

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace pxc::core {

struct Rect {
    int x = 0, y = 0, w = 0, h = 0;

    constexpr bool empty() const noexcept { return w <= 0 || h <= 0; }
    constexpr long long area() const noexcept { return empty() ? 0 : static_cast<long long>(w) * h; }
    constexpr int right() const noexcept { return x + w; }
    constexpr int bottom() const noexcept { return y + h; }
    constexpr bool contains(int px, int py) const noexcept {
        return px >= x && py >= y && px < right() && py < bottom();
    }
    constexpr Rect intersect(const Rect& o) const noexcept {
        const int l = std::max(x, o.x), t = std::max(y, o.y);
        const int r = std::min(right(), o.right()), b = std::min(bottom(), o.bottom());
        if (r <= l || b <= t) return {};
        return {l, t, r - l, b - t};
    }
    constexpr Rect unite(const Rect& o) const noexcept {
        if (empty()) return o;
        if (o.empty()) return *this;
        const int l = std::min(x, o.x), t = std::min(y, o.y);
        const int r = std::max(right(), o.right()), b = std::max(bottom(), o.bottom());
        return {l, t, r - l, b - t};
    }
    constexpr bool intersects(const Rect& o) const noexcept { return !intersect(o).empty(); }
    constexpr bool operator==(const Rect&) const = default;
};

struct Color {
    std::uint8_t r = 0, g = 0, b = 0, a = 255;

    constexpr bool operator==(const Color&) const = default;

    // Packed 0xAARRGGBB, which in little-endian memory is the byte order B, G, R, A.
    constexpr std::uint32_t toBGRA() const noexcept {
        return (std::uint32_t{a} << 24) | (std::uint32_t{r} << 16) | (std::uint32_t{g} << 8) | std::uint32_t{b};
    }
    static constexpr Color fromBGRA(std::uint32_t v) noexcept {
        return Color{static_cast<std::uint8_t>((v >> 16) & 0xFF), static_cast<std::uint8_t>((v >> 8) & 0xFF),
                     static_cast<std::uint8_t>(v & 0xFF), static_cast<std::uint8_t>((v >> 24) & 0xFF)};
    }
    static constexpr Color rgb(std::uint8_t r, std::uint8_t g, std::uint8_t b) noexcept { return {r, g, b, 255}; }

    // Accepts "#RRGGBB" and "#RRGGBBAA" (case-insensitive). Returns nullopt otherwise.
    static std::optional<Color> parseHex(std::string_view s) noexcept;
    std::string toHex() const; // "#RRGGBBAA"
};

inline constexpr Color kBlack{0, 0, 0, 255};
inline constexpr Color kWhite{255, 255, 255, 255};
inline constexpr Color kTransparent{0, 0, 0, 0};

} // namespace pxc::core
