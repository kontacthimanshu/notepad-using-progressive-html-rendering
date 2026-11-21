#include <cstdio>

#include "pxc/render/bmp_io.hpp"
#include "tool.hpp"

namespace pxc::tool {

int cmdCompare(const Args& a) {
    if (a.positional.size() != 2) {
        std::fputs("compare: expected <a.bmp> <b.bmp>\n", stderr);
        return 2;
    }
    auto x = render::readBmp32(a.positional[0]);
    auto y = render::readBmp32(a.positional[1]);
    if (!x || !y) {
        std::fprintf(stderr, "compare: %s\n", (!x ? x.error() : y.error()).c_str());
        return 2;
    }
    if (x->width() != y->width() || x->height() != y->height()) {
        std::printf("different: size %dx%d vs %dx%d\n", x->width(), x->height(), y->width(), y->height());
        return 1;
    }
    long long diffs = 0;
    render::Framebuffer diff(x->width(), x->height(), core::kWhite);
    for (int r = 0; r < x->height(); ++r)
        for (int c = 0; c < x->width(); ++c)
            if (x->pixel(c, r) != y->pixel(c, r)) {
                ++diffs;
                diff.setPixel(c, r, core::Color{255, 0, 255, 255}.toBGRA());
            }
    if (auto d = a.get("diff"); d && diffs > 0) (void)render::writeBmp32(diff, *d);
    if (diffs == 0) {
        std::puts("equal");
        return 0;
    }
    std::printf("different: %lld pixel(s)\n", diffs);
    return 1;
}

} // namespace pxc::tool
