#pragma once
// Pixel-exact golden-image comparison. PIXELCSS_UPDATE_GOLDENS=1 regenerates goldens (review them by eye).

#include <string>

#include <doctest.h>

#include "pxc/render/framebuffer.hpp"

namespace pxc::test {

struct GoldenResult {
    bool ok = false;
    std::string message;
};

// Compares `fb` with PXC_GOLDEN_DIR/<relativeName>. On mismatch writes actual.bmp and diff.bmp
// (mismatches in magenta) under PXC_GOLDEN_FAILURE_DIR/<relativeName stem>/.
GoldenResult compareToGolden(const render::Framebuffer& fb, const std::string& relativeName);

bool updateGoldensRequested();

} // namespace pxc::test

#define CHECK_GOLDEN(fb, name)                                                                       \
    do {                                                                                             \
        const auto pxc_golden_result_ = ::pxc::test::compareToGolden((fb), (name));                  \
        INFO(pxc_golden_result_.message);                                                            \
        CHECK(pxc_golden_result_.ok);                                                                \
    } while (0)
