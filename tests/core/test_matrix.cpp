#include <doctest/doctest.h>

#include "me/core/Matrix4x4.h"
#include "me/core/Vector2.h"
#include "me/core/MathConstants.h"

using me::Matrix4x4;
using me::Vector2;

TEST_CASE("identity transforms point unchanged") {
    Matrix4x4 id = Matrix4x4::Identity();
    Vector2 p = id.TransformPoint(Vector2{3.0f, 7.0f});
    CHECK(p.x == doctest::Approx(3.0f));
    CHECK(p.y == doctest::Approx(7.0f));
}

TEST_CASE("translation moves points but not vectors") {
    Matrix4x4 t = Matrix4x4::Translation(Vector2{10.0f, 5.0f});
    Vector2 p = t.TransformPoint(Vector2{1.0f, 1.0f});
    CHECK(p.x == doctest::Approx(11.0f));
    CHECK(p.y == doctest::Approx(6.0f));
    Vector2 v = t.TransformVector(Vector2{1.0f, 1.0f}); // 方向不受平移影响
    CHECK(v.x == doctest::Approx(1.0f));
    CHECK(v.y == doctest::Approx(1.0f));
}

TEST_CASE("scale scales points") {
    Matrix4x4 s = Matrix4x4::Scale(Vector2{2.0f, 3.0f});
    Vector2 p = s.TransformPoint(Vector2{4.0f, 5.0f});
    CHECK(p.x == doctest::Approx(8.0f));
    CHECK(p.y == doctest::Approx(15.0f));
}

TEST_CASE("rotation by 90deg is CCW (Y up)") {
    Matrix4x4 r = Matrix4x4::Rotation(me::kPi * 0.5f);
    Vector2 p = r.TransformPoint(Vector2{1.0f, 0.0f}); // (1,0) -> (0,1)
    CHECK(p.x == doctest::Approx(0.0f).epsilon(0.0001));
    CHECK(p.y == doctest::Approx(1.0f).epsilon(0.0001));
}

TEST_CASE("multiply applies left-to-right for row vectors") {
    // 行向量: p' = p * (S * T) => 先缩放后平移
    Matrix4x4 st = Matrix4x4::Scale(Vector2{2.0f, 2.0f}) * Matrix4x4::Translation(Vector2{1.0f, 1.0f});
    Vector2 p = st.TransformPoint(Vector2{3.0f, 4.0f}); // *2 -> (6,8) -> +1 -> (7,9)
    CHECK(p.x == doctest::Approx(7.0f));
    CHECK(p.y == doctest::Approx(9.0f));
}

TEST_CASE("orthographic maps screen rect to NDC") {
    // left=0,right=800,bottom=0,top=600 => (0,0)->(-1,-1), (800,600)->(1,1)
    Matrix4x4 o = Matrix4x4::Orthographic(0.0f, 800.0f, 0.0f, 600.0f, 0.0f, 1.0f);
    Vector2 lo = o.TransformPoint(Vector2{0.0f, 0.0f});
    Vector2 hi = o.TransformPoint(Vector2{800.0f, 600.0f});
    CHECK(lo.x == doctest::Approx(-1.0f));
    CHECK(lo.y == doctest::Approx(-1.0f));
    CHECK(hi.x == doctest::Approx(1.0f));
    CHECK(hi.y == doctest::Approx(1.0f));
}
