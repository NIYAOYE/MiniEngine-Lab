#include <doctest/doctest.h>

#include "me/scene/Scene.h"
#include "me/scene/Components.h"
#include "me/scene/RenderSystem.h"
#include "me/scene/TransformSystem.h"

using me::scene::Entity;
using me::scene::Scene;
using me::scene::CameraComponent;
using me::scene::CameraView;
using me::scene::RenderSystem;
using me::scene::TransformSystem;

namespace {
constexpr float kEps = 1e-4f;

Entity MakeCamera(Scene& s, float x, float y, float zoom) {
    const Entity e = s.CreateEntity();
    me::Transform2D t;
    t.position = me::Vector2{x, y};
    s.SetLocalTransform(e, t);
    CameraComponent c;
    c.zoom = zoom;
    c.viewportSize = me::Vector2{320.0f, 180.0f};
    s.AddComponent<CameraComponent>(e, c);
    return e;
}
} // namespace

TEST_CASE("Camera:无相机返回 nullopt") {
    Scene scene;
    CHECK_FALSE(RenderSystem::ResolveActiveCamera(scene).has_value());
}

TEST_CASE("Camera:解析活动相机——中心取实体世界位置") {
    Scene scene;
    const Entity cam = MakeCamera(scene, 64.0f, 32.0f, 2.0f);
    scene.SetActiveCamera(cam);
    TransformSystem::UpdateWorldTransforms(scene);
    const auto view = RenderSystem::ResolveActiveCamera(scene);
    REQUIRE(view.has_value());
    CHECK(view->center.x == doctest::Approx(64.0f).epsilon(kEps));
    CHECK(view->center.y == doctest::Approx(32.0f).epsilon(kEps));
    CHECK(view->zoom == doctest::Approx(2.0f).epsilon(kEps));
    CHECK(view->viewportSize.x == doctest::Approx(320.0f).epsilon(kEps));
}

TEST_CASE("Camera:未显式设置活动相机时取第一个相机") {
    Scene scene;
    MakeCamera(scene, 10.0f, 20.0f, 1.0f);
    TransformSystem::UpdateWorldTransforms(scene);
    const auto view = RenderSystem::ResolveActiveCamera(scene);
    REQUIRE(view.has_value());
    CHECK(view->center.x == doctest::Approx(10.0f).epsilon(kEps));
}

TEST_CASE("Scene:销毁活动相机实体后 ActiveCamera 失效不悬垂") {
    Scene scene;
    const Entity cam = MakeCamera(scene, 0.0f, 0.0f, 1.0f);
    scene.SetActiveCamera(cam);
    CHECK(scene.ActiveCamera() == cam);
    scene.DestroyEntity(cam);
    // 销毁后 ActiveCamera 必须已清除,不持有悬垂句柄。
    CHECK_FALSE(scene.ActiveCamera().IsValid());
}

TEST_CASE("Scene:销毁含活动相机子节点的父实体后 ActiveCamera 失效不悬垂") {
    Scene scene;
    const Entity parent = scene.CreateEntity();
    const Entity cam = MakeCamera(scene, 0.0f, 0.0f, 1.0f);
    scene.SetParent(cam, parent);
    scene.SetActiveCamera(cam);
    CHECK(scene.ActiveCamera() == cam);
    // 销毁父节点应连带销毁 cam,ActiveCamera 必须随之清除。
    scene.DestroyEntity(parent);
    CHECK_FALSE(scene.ActiveCamera().IsValid());
}

TEST_CASE("Camera:相机跟随父——世界位置含父平移") {
    Scene scene;
    const Entity player = scene.CreateEntity();
    me::Transform2D pt;
    pt.position = me::Vector2{200.0f, 0.0f};
    scene.SetLocalTransform(player, pt);
    const Entity cam = MakeCamera(scene, 0.0f, 0.0f, 1.0f);
    scene.SetParent(cam, player);
    scene.SetActiveCamera(cam);
    TransformSystem::UpdateWorldTransforms(scene);
    const auto view = RenderSystem::ResolveActiveCamera(scene);
    REQUIRE(view.has_value());
    CHECK(view->center.x == doctest::Approx(200.0f).epsilon(kEps));
}
