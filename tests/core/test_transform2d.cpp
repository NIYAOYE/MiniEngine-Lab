#include <doctest/doctest.h>

#include "me/core/Transform2D.h"
#include "me/core/Matrix4x4.h"
#include "me/core/Vector2.h"
#include "me/core/MathConstants.h"

using me::Transform2D;
using me::Vector2;

TEST_CASE("default transform is identity-like") {
    Transform2D t;
    CHECK(t.position == Vector2{0.0f, 0.0f});
    CHECK(t.rotation == doctest::Approx(0.0f));
    CHECK(t.scale == Vector2{1.0f, 1.0f});
    Vector2 p = t.ToMatrix().TransformPoint(Vector2{2.0f, 3.0f});
    CHECK(p.x == doctest::Approx(2.0f));
    CHECK(p.y == doctest::Approx(3.0f));
}

TEST_CASE("transform applies scale then translation") {
    Transform2D t;
    t.position = Vector2{10.0f, 5.0f};
    t.scale = Vector2{2.0f, 3.0f};
    Vector2 p = t.ToMatrix().TransformPoint(Vector2{1.0f, 1.0f});
    // 缩放 (2,3) -> (2,3) 再平移 (10,5) -> (12,8)
    CHECK(p.x == doctest::Approx(12.0f));
    CHECK(p.y == doctest::Approx(8.0f));
}

TEST_CASE("transform applies rotation about origin before translation") {
    Transform2D t;
    t.position = Vector2{5.0f, 0.0f};
    t.rotation = me::kPi * 0.5f; // 90deg CCW
    Vector2 p = t.ToMatrix().TransformPoint(Vector2{1.0f, 0.0f});
    // (1,0) 旋转 90 -> (0,1) 再平移 (5,0) -> (5,1)
    CHECK(p.x == doctest::Approx(5.0f).epsilon(0.0001));
    CHECK(p.y == doctest::Approx(1.0f).epsilon(0.0001));
}
