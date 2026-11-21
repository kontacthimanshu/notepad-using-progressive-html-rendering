#include <doctest.h>

#include <filesystem>

#include "pxc/fonts/font_registry.hpp"
#include "pxc/fonts/raster_cache.hpp"

using namespace pxc;
namespace fs = std::filesystem;

namespace {
struct MiniPack {
    core::DiagnosticsLog log;
    fonts::FontRegistry reg{log};
    MiniPack() { REQUIRE(reg.loadPack(fs::path(PXC_FIXTURES_DIR) / "packs/mini-pack") == fonts::PackState::Valid); }
};
} // namespace

TEST_CASE("first get rasterizes, the next 100 identical gets are hits") {
    MiniPack m;
    fonts::RasterCache cache(m.reg);
    const auto& g = cache.get("mini-pack", U'A', 1, core::kBlack, core::kWhite);
    CHECK(g.width == 3);
    CHECK(g.height == 5);
    CHECK(cache.stats().misses == 1);
    CHECK(cache.stats().rasterizations == 1);
    for (int i = 0; i < 100; ++i) (void)cache.get("mini-pack", U'A', 1, core::kBlack, core::kWhite);
    CHECK(cache.stats().hits == 100);
    CHECK(cache.stats().rasterizations == 1);
    CHECK(cache.stats().hitRate() == doctest::Approx(100.0 / 101.0));
}

TEST_CASE("scale, fg and bg are part of the key") {
    MiniPack m;
    fonts::RasterCache cache(m.reg);
    (void)cache.get("mini-pack", U'A', 1, core::kBlack, core::kWhite);
    (void)cache.get("mini-pack", U'A', 2, core::kBlack, core::kWhite);
    (void)cache.get("mini-pack", U'A', 1, core::Color{255, 0, 0, 255}, core::kWhite);
    (void)cache.get("mini-pack", U'A', 1, core::kBlack, core::Color{0, 0, 255, 255});
    CHECK(cache.stats().rasterizations == 4);
    CHECK(cache.entryCount() == 4);
    const auto& big = cache.get("mini-pack", U'A', 2, core::kBlack, core::kWhite);
    CHECK(big.width == 6);
    CHECK(cache.stats().rasterizations == 4);
}

TEST_CASE("LRU evicts the oldest entry when the budget is exceeded") {
    MiniPack m;
    const std::size_t oneEntry = sizeof(render::RasterGlyph) + 3 * 5 * 4;
    fonts::RasterCache cache(m.reg, oneEntry * 2);
    (void)cache.get("mini-pack", U'A', 1, core::kBlack, core::kWhite);
    (void)cache.get("mini-pack", U'B', 1, core::kBlack, core::kWhite);
    (void)cache.get("mini-pack", U'A', 1, core::kBlack, core::kWhite); // touch A
    (void)cache.get("mini-pack", U'C', 1, core::kBlack, core::kWhite); // evicts B
    CHECK(cache.stats().evictions == 1);
    CHECK(cache.entryCount() == 2);
    const auto before = cache.stats().rasterizations;
    (void)cache.get("mini-pack", U'A', 1, core::kBlack, core::kWhite);
    CHECK(cache.stats().rasterizations == before);
    (void)cache.get("mini-pack", U'B', 1, core::kBlack, core::kWhite);
    CHECK(cache.stats().rasterizations == before + 1);
}
