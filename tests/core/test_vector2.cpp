#include <doctest/doctest.h>

#include "me/core/Vector2.h"
#include "me/core/MathConstants.h"

using me::Vector2;

TEST_CASE("Vector2 default is zero") {
    Vector2 v;
    CHECK(v.x == doctest::Approx(0.0f));
    CHECK(v.y == doctest::Approx(0.0f));
}

TEST_CASE("Vector2 arithmetic") {
    Vector2 a{1.0f, 2.0f};
    Vector2 b{3.0f, 4.0f};
    CHECK((a + b) == Vector2{4.0f, 6.0f});
    CHECK((b - a) == Vector2{2.0f, 2.0f});
    CHECK((a * 2.0f) == Vector2{2.0f, 4.0f});
    CHECK((b / 2.0f) == Vector2{1.5f, 2.0f});
    CHECK((-a) == Vector2{-1.0f, -2.0f});
}

TEST_CASE("Vector2 compound assignment") {
    Vector2 a{1.0f, 1.0f};
    a += Vector2{2.0f, 3.0f};
    CHECK(a == Vector2{3.0f, 4.0f});
    a -= Vector2{1.0f, 1.0f};
    CHECK(a == Vector2{2.0f, 3.0f});
    a *= 2.0f;
    CHECK(a == Vector2{4.0f, 6.0f});
}

TEST_CASE("Vector2 length and dot") {
    Vector2 v{3.0f, 4.0f};
    CHECK(v.LengthSquared() == doctest::Approx(25.0f));
    CHECK(v.Length() == doctest::Approx(5.0f));
    CHECK(me::Dot(Vector2{1.0f, 0.0f}, Vector2{0.0f, 1.0f}) == doctest::Approx(0.0f));
    CHECK(me::Dot(Vector2{2.0f, 3.0f}, Vector2{4.0f, 5.0f}) == doctest::Approx(23.0f));
}

TEST_CASE("Vector2 normalized") {
    Vector2 n = Vector2{0.0f, 5.0f}.Normalized();
    CHECK(n.x == doctest::Approx(0.0f));
    CHECK(n.y == doctest::Approx(1.0f));
    // 零向量归一化返回零向量(不除零)
    Vector2 z = Vector2{0.0f, 0.0f}.Normalized();
    CHECK(z == Vector2{0.0f, 0.0f});
}
