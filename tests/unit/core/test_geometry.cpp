#include <doctest.h>

#include "pxc/core/geometry.hpp"

using namespace pxc::core;

TEST_CASE("rect intersect, unite, contains, area") {
    const Rect a{0, 0, 10, 10}, b{5, 5, 10, 10};
    CHECK(a.intersect(b) == Rect{5, 5, 5, 5});
    CHECK(a.unite(b) == Rect{0, 0, 15, 15});
    CHECK(a.contains(9, 9));
    CHECK_FALSE(a.contains(10, 0));
    CHECK(a.area() == 100);
    CHECK(Rect{0, 0, 0, 5}.empty());
    CHECK(a.intersect(Rect{20, 20, 1, 1}).empty());
    CHECK(Rect{}.unite(b) == b);
}

TEST_CASE("color hex parsing") {
    auto c = Color::parseHex("#FF8000");
    REQUIRE(c);
    CHECK(*c == Color{255, 128, 0, 255});
    auto d = Color::parseHex("#ff800040");
    REQUIRE(d);
    CHECK(d->a == 0x40);
    CHECK_FALSE(Color::parseHex("#FFF"));
    CHECK_FALSE(Color::parseHex("#GG0000"));
    CHECK_FALSE(Color::parseHex("FF0000"));
    CHECK(Color{1, 2, 3, 4}.toHex() == "#01020304");
}

TEST_CASE("color BGRA packing round-trips") {
    const Color c{0x12, 0x34, 0x56, 0x78};
    CHECK(c.toBGRA() == 0x78123456u);
    CHECK(Color::fromBGRA(c.toBGRA()) == c);
}
