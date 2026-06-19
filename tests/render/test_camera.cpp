#include <doctest/doctest.h>

#include "me/render/OrthographicCamera.h"
#include "me/core/Matrix4x4.h"
#include "me/core/Vector2.h"

using me::render::OrthographicCamera;
using me::Vector2;

namespace {
constexpr float kEps = 1e-4f;
constexpr float kW = 1280.0f;
constexpr float kH = 720.0f;
} // namespace

TEST_CASE("正交相机:位置映射到 NDC 原点,视口角点映射到 NDC 角点") {
    OrthographicCamera cam;
    cam.SetViewportSize(kW, kH);
    cam.SetPosition(Vector2{640.0f, 360.0f});
    cam.SetZoom(1.0f);
    const me::Matrix4x4 vp = cam.ViewProj();

    const Vector2 center = vp.TransformPoint(Vector2{640.0f, 360.0f});
    CHECK(center.x == doctest::Approx(0.0f).epsilon(kEps));
    CHECK(center.y == doctest::Approx(0.0f).epsilon(kEps));

    // 相机居中、zoom=1:可见范围正好一屏,右上角世界点 (1280,720) → NDC (1,1)。
    const Vector2 topRight = vp.TransformPoint(Vector2{1280.0f, 720.0f});
    CHECK(topRight.x == doctest::Approx(1.0f).epsilon(kEps));
    CHECK(topRight.y == doctest::Approx(1.0f).epsilon(kEps));
}

TEST_CASE("正交相机:zoom 放大使可见半宽减半") {
    OrthographicCamera cam;
    cam.SetViewportSize(kW, kH);
    cam.SetPosition(Vector2{640.0f, 360.0f});
    cam.SetZoom(2.0f);
    const me::Matrix4x4 vp = cam.ViewProj();

    // zoom=2:可见半宽=640/2=320,故世界点 (960,540) 落在 NDC (1,1)。
    const Vector2 edge = vp.TransformPoint(Vector2{960.0f, 540.0f});
    CHECK(edge.x == doctest::Approx(1.0f).epsilon(kEps));
    CHECK(edge.y == doctest::Approx(1.0f).epsilon(kEps));
}

TEST_CASE("正交相机:平移相机后世界原点不再是中心") {
    OrthographicCamera cam;
    cam.SetViewportSize(kW, kH);
    cam.SetPosition(Vector2{0.0f, 0.0f});
    cam.SetZoom(1.0f);
    const me::Matrix4x4 vp = cam.ViewProj();

    // 相机看向世界原点:世界 (0,0) → NDC 原点。
    const Vector2 c = vp.TransformPoint(Vector2{0.0f, 0.0f});
    CHECK(c.x == doctest::Approx(0.0f).epsilon(kEps));
    CHECK(c.y == doctest::Approx(0.0f).epsilon(kEps));
}
