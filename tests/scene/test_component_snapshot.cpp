#include <doctest/doctest.h>

#include "me/scene/Components.h"
#include "me/scene/Scene.h"

using me::scene::CameraComponent;
using me::scene::Entity;
using me::scene::Scene;
using me::scene::SpriteComponent;

TEST_CASE("Scene:CaptureComponents/RestoreComponents 跨实体还原组件数据") {
    Scene scene;
    const Entity src = scene.CreateEntity();
    SpriteComponent sprite;
    sprite.textureId = 7u;
    sprite.sortLayer = 3;
    scene.AddComponent<SpriteComponent>(src, sprite);
    CameraComponent cam;
    cam.zoom = 2.5f;
    scene.AddComponent<CameraComponent>(src, cam);

    auto snaps = scene.CaptureComponents(src);
    CHECK(snaps.size() == 2);

    const Entity dst = scene.CreateEntity();
    scene.RestoreComponents(dst, snaps);

    REQUIRE(scene.HasComponent<SpriteComponent>(dst));
    REQUIRE(scene.HasComponent<CameraComponent>(dst));
    CHECK(scene.GetComponent<SpriteComponent>(dst)->textureId == 7u);
    CHECK(scene.GetComponent<SpriteComponent>(dst)->sortLayer == 3);
    CHECK(scene.GetComponent<CameraComponent>(dst)->zoom == doctest::Approx(2.5f));
}

TEST_CASE("Scene:无组件实体 CaptureComponents 为空") {
    Scene scene;
    const Entity e = scene.CreateEntity();
    CHECK(scene.CaptureComponents(e).empty());
}

TEST_CASE("Scene:快照在捕获后与源组件隔离(值拷贝)") {
    Scene scene;
    const Entity src = scene.CreateEntity();
    SpriteComponent sprite;
    sprite.textureId = 1u;
    scene.AddComponent<SpriteComponent>(src, sprite);

    // 捕获快照(应持有 textureId=1 的值拷贝)。
    auto snaps = scene.CaptureComponents(src);

    // 修改源组件,验证快照不受影响。
    scene.GetComponent<SpriteComponent>(src)->textureId = 99u;
    CHECK(scene.GetComponent<SpriteComponent>(src)->textureId == 99u);

    // 还原快照到第二个实体,应得到捕获时的值 1,而非变更后的 99。
    const Entity dst = scene.CreateEntity();
    scene.RestoreComponents(dst, snaps);
    REQUIRE(scene.HasComponent<SpriteComponent>(dst));
    CHECK(scene.GetComponent<SpriteComponent>(dst)->textureId == 1u);
}
