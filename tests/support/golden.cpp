#include "support/golden.hpp"

#include <cstdlib>
#include <filesystem>

#include "pxc/render/bmp_io.hpp"

namespace pxc::test {

bool updateGoldensRequested() {
    char* v = nullptr;
    std::size_t len = 0;
    bool on = false;
    if (_dupenv_s(&v, &len, "PIXELCSS_UPDATE_GOLDENS") == 0 && v) {
        on = std::string(v) == "1";
        std::free(v);
    }
    return on;
}

GoldenResult compareToGolden(const render::Framebuffer& fb, const std::string& relativeName) {
    namespace fs = std::filesystem;
    const fs::path golden = fs::path(PXC_GOLDEN_DIR) / relativeName;

    if (updateGoldensRequested()) {
        auto w = render::writeBmp32(fb, golden);
        if (!w) return {false, "failed to write golden: " + w.error()};
        return {true, "golden updated: " + golden.generic_string()};
    }

    auto expected = render::readBmp32(golden);
    if (!expected) {
        return {false, "missing golden " + golden.generic_string() + " (" + expected.error() +
                           "); run with PIXELCSS_UPDATE_GOLDENS=1 to create it"};
    }

    const fs::path failDir = fs::path(PXC_GOLDEN_FAILURE_DIR) / fs::path(relativeName).replace_extension();
    if (expected->width() != fb.width() || expected->height() != fb.height()) {
        (void)render::writeBmp32(fb, failDir / "actual.bmp");
        return {false, "size mismatch for " + relativeName + ": expected " + std::to_string(expected->width()) + "x" +
                           std::to_string(expected->height()) + ", got " + std::to_string(fb.width()) + "x" +
                           std::to_string(fb.height())};
    }

    int mismatches = 0, firstX = -1, firstY = -1;
    render::Framebuffer diff(fb.width(), fb.height(), core::kWhite);
    for (int y = 0; y < fb.height(); ++y) {
        for (int x = 0; x < fb.width(); ++x) {
            if (fb.pixel(x, y) != expected->pixel(x, y)) {
                if (mismatches == 0) { firstX = x; firstY = y; }
                ++mismatches;
                diff.setPixel(x, y, core::Color{255, 0, 255, 255}.toBGRA());
            }
        }
    }
    if (mismatches == 0) return {true, "golden match: " + relativeName};

    (void)render::writeBmp32(fb, failDir / "actual.bmp");
    (void)render::writeBmp32(diff, failDir / "diff.bmp");
    return {false, relativeName + ": " + std::to_string(mismatches) + " pixel(s) differ; first at (" +
                       std::to_string(firstX) + "," + std::to_string(firstY) + ") expected " +
                       core::Color::fromBGRA(expected->pixel(firstX, firstY)).toHex() + " got " +
                       core::Color::fromBGRA(fb.pixel(firstX, firstY)).toHex() + "; see " + failDir.generic_string()};
}

} // namespace pxc::test
