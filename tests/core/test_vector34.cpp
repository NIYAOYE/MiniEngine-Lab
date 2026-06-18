#include <doctest/doctest.h>

#include "me/core/Vector3.h"
#include "me/core/Vector4.h"

using me::Vector3;
using me::Vector4;

TEST_CASE("Vector3 basics") {
    Vector3 a{1.0f, 2.0f, 2.0f};
    CHECK(a.LengthSquared() == doctest::Approx(9.0f));
    CHECK(a.Length() == doctest::Approx(3.0f));
    CHECK((a + Vector3{1.0f, 0.0f, 0.0f}) == Vector3{2.0f, 2.0f, 2.0f});
    CHECK((a * 2.0f) == Vector3{2.0f, 4.0f, 4.0f});
    CHECK(me::Dot(Vector3{1.0f, 2.0f, 3.0f}, Vector3{4.0f, 5.0f, 6.0f}) == doctest::Approx(32.0f));
}

TEST_CASE("Vector4 basics") {
    Vector4 c{1.0f, 0.5f, 0.25f, 1.0f};
    CHECK((c + Vector4{0.0f, 0.5f, 0.0f, 0.0f}) == Vector4{1.0f, 1.0f, 0.25f, 1.0f});
    CHECK((c * 2.0f) == Vector4{2.0f, 1.0f, 0.5f, 2.0f});
    CHECK(c == Vector4{1.0f, 0.5f, 0.25f, 1.0f});
}
