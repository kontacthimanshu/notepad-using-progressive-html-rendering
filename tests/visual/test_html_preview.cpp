// Visual test: HTML fixtures rendered through the shared glyph pipeline (FR-037, SC-007).
#include <doctest.h>

#include <filesystem>

#include "pxc/core/unicode.hpp"
#include "pxc/html/document_node.hpp"
#include "pxc/update/preview_painter.hpp"
#include "support/fs_util.hpp"
#include "support/golden.hpp"

using namespace pxc;
namespace fs = std::filesystem;

namespace {
render::Framebuffer renderFixture(const std::string& name) {
    core::DiagnosticsLog log;
    fonts::FontRegistry reg(log);
    reg.loadAll(PXC_FONTS_DIR);
    fonts::RasterCache cache(reg);
    const auto src = *core::decodeUtf8(test::readAll(fs::path(PXC_FIXTURES_DIR) / "html" / (name + ".html")),
                                       core::InvalidPolicy::Replace);
    auto doc = std::move(*html::parseHtml(src));
    html::computeStyles(doc, reg, "verdana-style", log);
    update::PreviewPaintState st;
    if (doc.hasErrors()) st.banner = U"This document has HTML errors - see Help > Diagnostics";
    const int banner = update::previewBannerHeight(st, reg);
    const auto lay = layout::layoutBlocks(html::toStyledBlocks(doc), reg, 480, 1);
    render::Framebuffer fb(480, std::max(40, lay.height + banner + 8));
    render::Renderer r;
    update::paintPreview(fb, fb.bounds(), lay, st, reg, cache, r, update::Theme{});
    return fb;
}
} // namespace

TEST_CASE("HTML fixtures render pixel-exactly") {
    for (const char* name : {"basic", "nested", "styles", "unsupported", "malformed-unclosed-attr", "malformed-misnest",
                             "malformed-stray-end"}) {
        CAPTURE(name);
        CHECK_GOLDEN(renderFixture(name), std::string("html/") + name + ".bmp");
    }
}
