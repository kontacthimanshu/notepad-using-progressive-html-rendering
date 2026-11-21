#include "pxc/render/framebuffer.hpp"

#include <algorithm>

namespace pxc::render {

Framebuffer::Framebuffer(int w, int h, Color fill) { (void)resize(w, h, fill); }

core::Expected<Framebuffer, std::string> Framebuffer::create(int w, int h, Color fill) {
    Framebuffer fb;
    auto r = fb.resize(w, h, fill);
    if (!r) return core::Unexpected(r.error());
    return fb;
}

core::Expected<void, std::string> Framebuffer::resize(int w, int h, Color fill) {
    if (w < 0 || h < 0 || w > kMaxFramebufferDim || h > kMaxFramebufferDim) {
        w_ = h_ = 0;
        px_.clear();
        return core::Unexpected(std::string("framebuffer dimensions out of range: ") + std::to_string(w) + "x" +
                                std::to_string(h));
    }
    w_ = w;
    h_ = h;
    px_.assign(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), fill.toBGRA());
    return {};
}

void Framebuffer::fillRect(Rect r, Color c) noexcept {
    const Rect clip = r.intersect(bounds());
    if (clip.empty()) return;
    const std::uint32_t v = c.toBGRA();
    for (int y = clip.y; y < clip.bottom(); ++y) {
        auto rw = row(y);
        std::fill(rw.begin() + clip.x, rw.begin() + clip.right(), v);
    }
}

} // namespace pxc::render
