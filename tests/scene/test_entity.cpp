#include <doctest/doctest.h>

#include "me/scene/Scene.h"

using me::scene::Entity;
using me::scene::Scene;

TEST_CASE("Scene:新建实体存活、计数递增") {
    Scene scene;
    CHECK(scene.AliveCount() == 0);
    const Entity a = scene.CreateEntity();
    const Entity b = scene.CreateEntity();
    CHECK(a.IsValid());
    CHECK(a != b);
    CHECK(scene.IsAlive(a));
    CHECK(scene.IsAlive(b));
    CHECK(scene.AliveCount() == 2);
}

TEST_CASE("Scene:销毁后句柄失效、计数递减") {
    Scene scene;
    const Entity a = scene.CreateEntity();
    scene.DestroyEntity(a);
    CHECK_FALSE(scene.IsAlive(a));
    CHECK(scene.AliveCount() == 0);
}

TEST_CASE("Scene:槽位复用——旧句柄因 generation 不匹配而失效") {
    Scene scene;
    const Entity a = scene.CreateEntity();
    scene.DestroyEntity(a);
    const Entity b = scene.CreateEntity(); // 复用同一槽位
    CHECK(b.index == a.index);             // 同槽位
    CHECK(b.generation != a.generation);   // 代号已递增
    CHECK_FALSE(scene.IsAlive(a));         // 旧句柄悬垂
    CHECK(scene.IsAlive(b));
}

TEST_CASE("Scene:AliveEntities 只含存活实体") {
    Scene scene;
    const Entity a = scene.CreateEntity();
    const Entity b = scene.CreateEntity();
    scene.DestroyEntity(a);
    const auto alive = scene.AliveEntities();
    REQUIRE(alive.size() == 1);
    CHECK(alive[0] == b);
}

TEST_CASE("Scene:对失效句柄 DestroyEntity 安全无副作用") {
    Scene scene;
    const Entity a = scene.CreateEntity();
    scene.DestroyEntity(a);
    scene.DestroyEntity(a); // 重复销毁不崩溃、不改变计数
    CHECK(scene.AliveCount() == 0);
}
