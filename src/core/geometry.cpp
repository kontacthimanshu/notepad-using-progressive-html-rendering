#include "pxc/core/geometry.hpp"

namespace pxc::core {

namespace {
int hexVal(char c) noexcept {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
std::optional<std::uint8_t> byteAt(std::string_view s, std::size_t i) noexcept {
    const int hi = hexVal(s[i]), lo = hexVal(s[i + 1]);
    if (hi < 0 || lo < 0) return std::nullopt;
    return static_cast<std::uint8_t>(hi * 16 + lo);
}
} // namespace

std::optional<Color> Color::parseHex(std::string_view s) noexcept {
    if (s.size() != 7 && s.size() != 9) return std::nullopt;
    if (s[0] != '#') return std::nullopt;
    auto r = byteAt(s, 1), g = byteAt(s, 3), b = byteAt(s, 5);
    if (!r || !g || !b) return std::nullopt;
    std::uint8_t a = 255;
    if (s.size() == 9) {
        auto av = byteAt(s, 7);
        if (!av) return std::nullopt;
        a = *av;
    }
    return Color{*r, *g, *b, a};
}

std::string Color::toHex() const {
    static constexpr char digits[] = "0123456789ABCDEF";
    std::string out = "#";
    for (std::uint8_t v : {r, g, b, a}) {
        out += digits[v >> 4];
        out += digits[v & 0xF];
    }
    return out;
}

} // namespace pxc::core
