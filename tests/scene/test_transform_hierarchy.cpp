#include <doctest/doctest.h>

#include "me/scene/Scene.h"
#include "me/scene/TransformSystem.h"

using me::scene::Entity;
using me::scene::Scene;
using me::scene::TransformSystem;

namespace {
constexpr float kEps = 1e-4f;

me::Transform2D At(float x, float y) {
    me::Transform2D t;
    t.position = me::Vector2{x, y};
    return t;
}
} // namespace

TEST_CASE("Transform:无父实体世界平移=局部平移") {
    Scene scene;
    const Entity e = scene.CreateEntity();
    scene.SetLocalTransform(e, At(10.0f, 5.0f));
    TransformSystem::UpdateWorldTransforms(scene);
    const me::Vector2 p = scene.WorldMatrix(e).TransformPoint(me::Vector2{0.0f, 0.0f});
    CHECK(p.x == doctest::Approx(10.0f).epsilon(kEps));
    CHECK(p.y == doctest::Approx(5.0f).epsilon(kEps));
}

TEST_CASE("Transform:子世界=局部叠加父平移") {
    Scene scene;
    const Entity parent = scene.CreateEntity();
    const Entity child = scene.CreateEntity();
    scene.SetLocalTransform(parent, At(10.0f, 0.0f));
    scene.SetLocalTransform(child, At(5.0f, 0.0f));
    scene.SetParent(child, parent);
    TransformSystem::UpdateWorldTransforms(scene);
    const me::Vector2 p = scene.WorldMatrix(child).TransformPoint(me::Vector2{0.0f, 0.0f});
    CHECK(p.x == doctest::Approx(15.0f).epsilon(kEps)); // 5 + 10
    CHECK(p.y == doctest::Approx(0.0f).epsilon(kEps));
}

TEST_CASE("Transform:父缩放作用于子(world = local * parentWorld)") {
    Scene scene;
    const Entity parent = scene.CreateEntity();
    const Entity child = scene.CreateEntity();
    me::Transform2D pt = At(10.0f, 0.0f);
    pt.scale = me::Vector2{2.0f, 2.0f};
    scene.SetLocalTransform(parent, pt);
    scene.SetLocalTransform(child, At(5.0f, 0.0f));
    scene.SetParent(child, parent);
    TransformSystem::UpdateWorldTransforms(scene);
    // child 原点 → 局部(5,0) 经父缩放2+平移10 → (5*2+10, 0) = (20,0)
    const me::Vector2 p = scene.WorldMatrix(child).TransformPoint(me::Vector2{0.0f, 0.0f});
    CHECK(p.x == doctest::Approx(20.0f).epsilon(kEps));
}

TEST_CASE("Transform:移动父→子脏标记重算") {
    Scene scene;
    const Entity parent = scene.CreateEntity();
    const Entity child = scene.CreateEntity();
    scene.SetLocalTransform(parent, At(10.0f, 0.0f));
    scene.SetLocalTransform(child, At(5.0f, 0.0f));
    scene.SetParent(child, parent);
    TransformSystem::UpdateWorldTransforms(scene);
    // 移动父:子的世界缓存应被标记脏并在下次更新重算。
    scene.SetLocalTransform(parent, At(100.0f, 0.0f));
    CHECK(scene.IsWorldDirty(child));
    TransformSystem::UpdateWorldTransforms(scene);
    const me::Vector2 p = scene.WorldMatrix(child).TransformPoint(me::Vector2{0.0f, 0.0f});
    CHECK(p.x == doctest::Approx(105.0f).epsilon(kEps));
}

TEST_CASE("Transform:SetParent 维护 children 邻接") {
    Scene scene;
    const Entity parent = scene.CreateEntity();
    const Entity child = scene.CreateEntity();
    scene.SetParent(child, parent);
    REQUIRE(scene.Children(parent).size() == 1);
    CHECK(scene.Children(parent)[0] == child);
    CHECK(scene.Parent(child) == parent);
    // 脱离父
    scene.SetParent(child, Entity::Invalid());
    CHECK(scene.Children(parent).empty());
    CHECK_FALSE(scene.Parent(child).IsValid());
}
