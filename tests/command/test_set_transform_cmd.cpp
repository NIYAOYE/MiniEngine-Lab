#include <doctest/doctest.h>

#include "me/command/CommandStack.h"
#include "me/command/commands/SetTransformCmd.h"
#include "me/core/Transform2D.h"
#include "me/scene/Scene.h"

using me::command::CommandStack;
using me::command::SetTransformCmd;
using me::scene::Entity;
using me::scene::EntityId;
using me::scene::Scene;

TEST_CASE("SetTransformCmd:execute 改变、undo 还原、redo 再变") {
    Scene scene;
    CommandStack stack;
    const Entity e = scene.CreateEntity();
    const EntityId id = scene.IdOf(e);

    me::Transform2D next;
    next.position = me::Vector2{10.0f, 20.0f};
    next.rotation = 1.5f;

    CHECK(stack.execute(std::make_unique<SetTransformCmd>(id, next), scene).ok);
    CHECK(scene.LocalTransform(e).position.x == doctest::Approx(10.0f));
    CHECK(scene.LocalTransform(e).rotation == doctest::Approx(1.5f));

    CHECK(stack.undo(scene).ok);
    CHECK(scene.LocalTransform(e).position.x == doctest::Approx(0.0f));
    CHECK(scene.LocalTransform(e).rotation == doctest::Approx(0.0f));

    CHECK(stack.redo(scene).ok);
    CHECK(scene.LocalTransform(e).position.y == doctest::Approx(20.0f));
}

TEST_CASE("SetTransformCmd:对失活实体 execute 返回失败") {
    Scene scene;
    CommandStack stack;
    const Entity e = scene.CreateEntity();
    const EntityId id = scene.IdOf(e);
    scene.DestroyEntity(e);

    me::Transform2D next;
    next.position = me::Vector2{5.0f, 5.0f};
    const auto r = stack.execute(std::make_unique<SetTransformCmd>(id, next), scene);
    CHECK_FALSE(r.ok);
    CHECK_FALSE(stack.canUndo());
}
