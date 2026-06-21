#include <doctest/doctest.h>

#include "me/command/CommandStack.h"
#include "me/command/commands/DestroyEntityCmd.h"
#include "me/scene/Components.h"
#include "me/scene/Scene.h"

using me::command::CommandStack;
using me::command::DestroyEntityCmd;
using me::scene::CameraComponent;
using me::scene::Entity;
using me::scene::EntityId;
using me::scene::Scene;
using me::scene::SpriteComponent;

TEST_CASE("DestroyEntityCmd:undo 还原子树/层级/局部变换/组件/active camera") {
    Scene scene;
    CommandStack stack;

    // 父 - 子 - 孙;父带 Sprite,子设为 active camera 带 CameraComponent。
    const Entity parent = scene.CreateEntity();
    const Entity child = scene.CreateEntity();
    const Entity grand = scene.CreateEntity();
    scene.SetParent(child, parent);
    scene.SetParent(grand, child);

    me::Transform2D ct;
    ct.position = me::Vector2{4.0f, 6.0f};
    scene.SetLocalTransform(child, ct);

    SpriteComponent sprite;
    sprite.textureId = 9u;
    scene.AddComponent<SpriteComponent>(parent, sprite);
    CameraComponent cam;
    cam.zoom = 3.0f;
    scene.AddComponent<CameraComponent>(child, cam);
    scene.SetActiveCamera(child);

    const EntityId pid = scene.IdOf(parent);
    const EntityId cid = scene.IdOf(child);
    const EntityId gid = scene.IdOf(grand);

    CHECK(stack.execute(std::make_unique<DestroyEntityCmd>(pid), scene).ok);
    CHECK(scene.AliveCount() == 0);
    CHECK_FALSE(scene.ActiveCamera().IsValid());

    CHECK(stack.undo(scene).ok);
    CHECK(scene.AliveCount() == 3);

    const Entity p2 = scene.Resolve(pid);
    const Entity c2 = scene.Resolve(cid);
    const Entity g2 = scene.Resolve(gid);
    REQUIRE(p2.IsValid());
    REQUIRE(c2.IsValid());
    REQUIRE(g2.IsValid());

    // 层级还原
    CHECK(scene.Parent(c2) == p2);
    CHECK(scene.Parent(g2) == c2);
    // 局部变换还原
    CHECK(scene.LocalTransform(c2).position.x == doctest::Approx(4.0f));
    // 组件还原
    REQUIRE(scene.HasComponent<SpriteComponent>(p2));
    CHECK(scene.GetComponent<SpriteComponent>(p2)->textureId == 9u);
    REQUIRE(scene.HasComponent<CameraComponent>(c2));
    CHECK(scene.GetComponent<CameraComponent>(c2)->zoom == doctest::Approx(3.0f));
    // active camera 还原
    CHECK(scene.ActiveCamera() == c2);
}

TEST_CASE("DestroyEntityCmd:redo 再次销毁后二次 undo 仍正确") {
    Scene scene;
    CommandStack stack;
    const Entity parent = scene.CreateEntity();
    const Entity child = scene.CreateEntity();
    scene.SetParent(child, parent);
    const EntityId pid = scene.IdOf(parent);
    const EntityId cid = scene.IdOf(child);

    stack.execute(std::make_unique<DestroyEntityCmd>(pid), scene);
    stack.undo(scene);
    CHECK(stack.redo(scene).ok);            // 再次销毁
    CHECK(scene.AliveCount() == 0);
    CHECK(stack.undo(scene).ok);            // 二次还原
    CHECK(scene.AliveCount() == 2);
    CHECK(scene.Parent(scene.Resolve(cid)) == scene.Resolve(pid));
}
