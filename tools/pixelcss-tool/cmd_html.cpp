// `html`: headless read-only preview render of a local HTML file to a BMP (FR-035..FR-041).
// Scripts never run and nothing is fetched: the engine has no network or script code path.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>

#include "pxc/core/unicode.hpp"
#include "pxc/html/document_node.hpp"
#include "pxc/render/bmp_io.hpp"
#include "pxc/update/preview_painter.hpp"
#include "tool.hpp"

namespace pxc::tool {

int cmdHtml(const Args& a) {
    const auto in = a.get("in");
    const auto out = a.get("out");
    if (!in || !out) {
        std::fputs("html: --in and --out are required\n", stderr);
        return 2;
    }
    const int width = a.get("width") ? std::atoi(a.get("width")->c_str()) : 480;
    const std::string pack = a.get("pack").value_or("verdana-style");
    std::ifstream f(std::filesystem::path(core::utf8ToWide(*in)), std::ios::binary);
    if (!f) {
        std::fprintf(stderr, "html: cannot open %s\n", in->c_str());
        return 1;
    }
    const std::string bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

    core::DiagnosticsLog log;
    fonts::FontRegistry reg(log);
    reg.loadAll(a.get("fonts-dir") ? std::filesystem::path(*a.get("fonts-dir")) : defaultFontsDir());
    fonts::RasterCache cache(reg);
    auto doc = std::move(*html::parseHtml(*core::decodeUtf8(bytes, core::InvalidPolicy::Replace)));
    html::computeStyles(doc, reg, pack, log);
    update::PreviewPaintState st;
    if (doc.hasErrors()) st.banner = U"This document has HTML errors - see Help > Diagnostics";
    const int banner = update::previewBannerHeight(st, reg);
    const auto lay = layout::layoutBlocks(html::toStyledBlocks(doc), reg, std::max(16, width), 1);
    render::Framebuffer fb(std::max(16, width), std::max(40, lay.height + banner + 8));
    render::Renderer r;
    update::paintPreview(fb, fb.bounds(), lay, st, reg, cache, r, update::Theme{});
    for (const auto& d : doc.diagnostics) std::printf("%s\n", core::format(d).c_str());
    for (const auto& d : log.all()) std::printf("%s\n", core::format(d).c_str());
    auto w = render::writeBmp32(fb, *out);
    if (!w) {
        std::fprintf(stderr, "html: %s\n", w.error().c_str());
        return 1;
    }
    std::printf("wrote %s (%dx%d)\n", out->c_str(), fb.width(), fb.height());
    return 0;
}

} // namespace pxc::tool
