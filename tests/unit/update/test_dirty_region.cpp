#include <doctest.h>

#include "pxc/update/dirty_region.hpp"

using namespace pxc;
using namespace pxc::update;
using core::Rect;

TEST_CASE("adding and merging overlapping rectangles") {
    DirtyRegion d;
    d.setBounds({0, 0, 100, 100});
    d.add({0, 0, 10, 10});
    d.add({5, 5, 10, 10}); // overlaps -> merged
    REQUIRE(d.rects().size() == 1);
    CHECK(d.rects()[0] == Rect{0, 0, 15, 15});
    d.add({50, 50, 5, 5}); // disjoint
    CHECK(d.rects().size() == 2);
    CHECK(d.area() == 15 * 15 + 25);
    d.add({-10, 90, 20, 50}); // clipped to bounds
    CHECK(d.boundingBox() == Rect{0, 0, 55, 100});
}

TEST_CASE("the 17th disjoint rectangle collapses the region into its bounding box") {
    DirtyRegion d;
    d.setBounds({0, 0, 1000, 1000});
    for (int i = 0; i < 16; ++i) d.add({i * 20, i * 20, 5, 5});
    CHECK(d.rects().size() == 16);
    d.add({900, 900, 5, 5});
    REQUIRE(d.rects().size() == 1);
    CHECK(d.rects()[0] == Rect{0, 0, 905, 905});
}

TEST_CASE("fullViewport overrides rectangles; clear empties the region") {
    DirtyRegion d;
    d.setBounds({0, 0, 50, 40});
    d.add({1, 1, 2, 2});
    d.markFull();
    CHECK(d.full());
    CHECK(d.area() == 50 * 40);
    CHECK(d.boundingBox() == Rect{0, 0, 50, 40});
    d.add({3, 3, 3, 3}); // ignored while full
    d.clear();
    CHECK(d.empty());
    CHECK_FALSE(d.full());
    CHECK(d.rects().empty());
}

TEST_CASE("shifting rectangles for a scroll") {
    DirtyRegion d;
    d.setBounds({0, 0, 100, 100});
    d.add({0, 40, 100, 10});
    d.shiftY(-15);
    REQUIRE(d.rects().size() == 1);
    CHECK(d.rects()[0] == Rect{0, 25, 100, 10});
    d.shiftY(-30); // partly scrolled off the top -> clipped
    CHECK(d.rects()[0] == Rect{0, 0, 100, 5});
    d.shiftY(-20); // entirely off -> dropped
    CHECK(d.rects().empty());
}
