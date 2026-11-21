#include <cstdio>
#include <cstdlib>

#include "pxc/core/unicode.hpp"
#include "pxc/fonts/font_registry.hpp"
#include "pxc/fonts/raster_cache.hpp"
#include "pxc/fonts/text_line.hpp"
#include "pxc/render/bmp_io.hpp"
#include "tool.hpp"

namespace pxc::tool {

int cmdRender(const Args& a) {
    const auto pack = a.get("pack");
    const auto text = a.get("text");
    const auto out = a.get("out");
    if (!pack || !text || !out) {
        std::fputs("render: --pack, --text and --out are required\n", stderr);
        return 2;
    }
    const int scale = a.get("scale") ? std::atoi(a.get("scale")->c_str()) : 1;
    auto fg = core::Color::parseHex(a.get("fg").value_or("#000000"));
    auto bg = core::Color::parseHex(a.get("bg").value_or("#FFFFFF"));
    if (!fg || !bg || scale < 1 || scale > 16) {
        std::fputs("render: invalid --fg/--bg colour or --scale (1..16)\n", stderr);
        return 2;
    }
    core::DiagnosticsLog log;
    fonts::FontRegistry reg(log);
    reg.loadAll(a.get("fonts-dir") ? std::filesystem::path(*a.get("fonts-dir")) : defaultFontsDir());
    if (!reg.isUsable(*pack)) {
        std::fprintf(stderr, "render: pack \"%s\" is not available\n", pack->c_str());
        for (const auto& d : log.all())
            if (d.severity == core::Severity::Error) std::fprintf(stderr, "%s\n", core::format(d).c_str());
        return 1;
    }
    fonts::RasterCache cache(reg);
    const std::u32string u = *core::decodeUtf8(*text, core::InvalidPolicy::Replace);
    const render::Framebuffer fb = fonts::renderTextLine(reg, cache, *pack, u, scale, *fg, *bg);
    auto w = render::writeBmp32(fb, *out);
    if (!w) {
        std::fprintf(stderr, "render: %s\n", w.error().c_str());
        return 1;
    }
    std::printf("wrote %s (%dx%d)\n", out->c_str(), fb.width(), fb.height());
    return 0;
}

} // namespace pxc::tool
