#include <doctest/doctest.h>

#include "me/core/Rect.h"
#include "me/core/AABB.h"
#include "me/core/Vector2.h"

using me::Rect;
using me::AABB;
using me::Vector2;

TEST_CASE("rect contains point") {
    Rect r{0.0f, 0.0f, 10.0f, 4.0f};
    CHECK(r.Contains(Vector2{5.0f, 2.0f}));
    CHECK(r.Contains(Vector2{0.0f, 0.0f}));   // 左下边界含
    CHECK_FALSE(r.Contains(Vector2{10.0f, 2.0f})); // 右上边界不含(半开区间)
    CHECK_FALSE(r.Contains(Vector2{-1.0f, 2.0f}));
    CHECK(r.Min() == Vector2{0.0f, 0.0f});
    CHECK(r.Max() == Vector2{10.0f, 4.0f});
}

TEST_CASE("rect intersects") {
    Rect a{0.0f, 0.0f, 10.0f, 10.0f};
    Rect b{5.0f, 5.0f, 10.0f, 10.0f};
    Rect c{20.0f, 20.0f, 1.0f, 1.0f};
    CHECK(a.Intersects(b));
    CHECK_FALSE(a.Intersects(c));
}

TEST_CASE("aabb center extents and contains") {
    AABB box{Vector2{-2.0f, -4.0f}, Vector2{2.0f, 4.0f}};
    CHECK(box.Center() == Vector2{0.0f, 0.0f});
    CHECK(box.Extents() == Vector2{2.0f, 4.0f});
    CHECK(box.Contains(Vector2{0.0f, 0.0f}));
    CHECK_FALSE(box.Contains(Vector2{3.0f, 0.0f}));
}

TEST_CASE("aabb intersects and expand") {
    AABB a{Vector2{0.0f, 0.0f}, Vector2{4.0f, 4.0f}};
    AABB b{Vector2{2.0f, 2.0f}, Vector2{6.0f, 6.0f}};
    AABB far{Vector2{10.0f, 10.0f}, Vector2{12.0f, 12.0f}};
    CHECK(a.Intersects(b));
    CHECK_FALSE(a.Intersects(far));

    AABB e = a.Expanded(Vector2{-1.0f, 5.0f});
    CHECK(e.min == Vector2{-1.0f, 0.0f});
    CHECK(e.max == Vector2{4.0f, 5.0f});
}
