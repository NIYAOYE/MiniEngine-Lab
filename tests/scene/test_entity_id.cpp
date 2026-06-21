#include <doctest/doctest.h>

#include "me/scene/Scene.h"

using me::scene::Entity;
using me::scene::EntityId;
using me::scene::Scene;

TEST_CASE("Scene:EntityId 单调递增且与句柄互相解析") {
    Scene scene;
    const Entity a = scene.CreateEntity();
    const Entity b = scene.CreateEntity();
    const EntityId ida = scene.IdOf(a);
    const EntityId idb = scene.IdOf(b);
    CHECK(ida != 0);
    CHECK(idb != 0);
    CHECK(ida != idb);
    CHECK(scene.Resolve(ida) == a);
    CHECK(scene.Resolve(idb) == b);
}

TEST_CASE("Scene:销毁后 IdOf 归零、Resolve 失效") {
    Scene scene;
    const Entity a = scene.CreateEntity();
    const EntityId ida = scene.IdOf(a);
    scene.DestroyEntity(a);
    CHECK(scene.IdOf(a) == 0);
    CHECK_FALSE(scene.Resolve(ida).IsValid());
}

TEST_CASE("Scene:CreateEntityWithId 以原 id 重建——身份锚定不随 generation 变化") {
    Scene scene;
    const Entity a = scene.CreateEntity();
    const EntityId ida = scene.IdOf(a);
    scene.DestroyEntity(a);

    const Entity restored = scene.CreateEntityWithId(ida);
    CHECK(scene.IsAlive(restored));
    CHECK(scene.IdOf(restored) == ida);     // 同一逻辑身份
    CHECK(scene.Resolve(ida) == restored);  // 解析到新句柄
    CHECK(restored != a);                    // generation 已变,handle 不同
}

TEST_CASE("Scene:CreateEntityWithId 后续 CreateEntity 不与其 id 冲突") {
    Scene scene;
    const Entity a = scene.CreateEntity();
    const EntityId ida = scene.IdOf(a);
    scene.DestroyEntity(a);
    scene.CreateEntityWithId(ida);
    const Entity c = scene.CreateEntity();
    CHECK(scene.IdOf(c) != ida);
}
