#pragma once
// Integer document scale policy (research R-08): s = max(1, floor(dpi / 96 + 0.5)).
// 100% -> 1, 125% -> 1, 150% -> 2, 175% -> 2, 200% -> 2, 250% -> 3. Nearest-neighbour cell replication keeps
// glyph pixels crisp and deterministic (FR-015, FR-022).

namespace pxc::core {

constexpr int scaleForDpi(unsigned dpi) noexcept {
    const int s = static_cast<int>((dpi * 2u + 96u) / 192u);
    return s < 1 ? 1 : s;
}

} // namespace pxc::core
