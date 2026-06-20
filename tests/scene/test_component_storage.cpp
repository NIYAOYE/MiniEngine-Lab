#include <doctest/doctest.h>

#include "me/scene/Scene.h"

using me::scene::Entity;
using me::scene::Scene;

namespace {
struct Health { int value = 0; };
struct Tag { };
} // namespace

TEST_CASE("Components:增/查/取") {
    Scene scene;
    const Entity e = scene.CreateEntity();
    CHECK_FALSE(scene.HasComponent<Health>(e));
    scene.AddComponent<Health>(e, Health{42});
    CHECK(scene.HasComponent<Health>(e));
    REQUIRE(scene.GetComponent<Health>(e) != nullptr);
    CHECK(scene.GetComponent<Health>(e)->value == 42);
}

TEST_CASE("Components:多类型互不干扰") {
    Scene scene;
    const Entity e = scene.CreateEntity();
    scene.AddComponent<Health>(e, Health{1});
    scene.AddComponent<Tag>(e, Tag{});
    CHECK(scene.HasComponent<Health>(e));
    CHECK(scene.HasComponent<Tag>(e));
    CHECK_FALSE(scene.HasComponent<Tag>(scene.CreateEntity()));
}

TEST_CASE("Components:移除") {
    Scene scene;
    const Entity e = scene.CreateEntity();
    scene.AddComponent<Health>(e, Health{7});
    scene.RemoveComponent<Health>(e);
    CHECK_FALSE(scene.HasComponent<Health>(e));
    CHECK(scene.GetComponent<Health>(e) == nullptr);
}

TEST_CASE("Components:销毁实体自动清理其组件") {
    Scene scene;
    const Entity a = scene.CreateEntity();
    const Entity b = scene.CreateEntity();
    scene.AddComponent<Health>(a, Health{10});
    scene.AddComponent<Health>(b, Health{20});
    scene.DestroyEntity(a);
    // a 的组件已随实体销毁;b 不受影响。
    CHECK(scene.ComponentStore<Health>().Size() == 1);
    REQUIRE(scene.GetComponent<Health>(b) != nullptr);
    CHECK(scene.GetComponent<Health>(b)->value == 20);
}

TEST_CASE("Components:槽位复用后新实体不继承旧组件") {
    Scene scene;
    const Entity a = scene.CreateEntity();
    scene.AddComponent<Health>(a, Health{99});
    scene.DestroyEntity(a);
    const Entity b = scene.CreateEntity(); // 复用槽位
    CHECK_FALSE(scene.HasComponent<Health>(b));
}

TEST_CASE("Components:用悬垂句柄 RemoveComponent 不误删复用槽位的新组件") {
    Scene scene;
    const Entity a = scene.CreateEntity();
    scene.AddComponent<Health>(a, Health{5});
    scene.DestroyEntity(a);
    const Entity b = scene.CreateEntity(); // 复用 a 的槽位,新 generation
    scene.AddComponent<Health>(b, Health{9});
    // 用 a 的悬垂句柄尝试删除;应该被拒绝(generation 不匹配)
    scene.RemoveComponent<Health>(a);
    // b 的组件应该完好无损
    CHECK(scene.HasComponent<Health>(b));
    REQUIRE(scene.GetComponent<Health>(b) != nullptr);
    CHECK(scene.GetComponent<Health>(b)->value == 9);
}
