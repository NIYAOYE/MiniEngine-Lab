#include <doctest/doctest.h>

#include "me/command/CommandStack.h"
#include "me/command/commands/CreateEntityCmd.h"
#include "me/scene/Scene.h"

using me::command::CommandStack;
using me::command::CreateEntityCmd;
using me::scene::EntityId;
using me::scene::Scene;

TEST_CASE("CreateEntityCmd:execute 创建、undo 销毁、redo 以原 id 重建") {
    Scene scene;
    CommandStack stack;
    auto cmd = std::make_unique<CreateEntityCmd>();
    CreateEntityCmd* raw = cmd.get();

    CHECK(stack.execute(std::move(cmd), scene).ok);
    const EntityId id = raw->CreatedId();
    CHECK(id != 0);
    CHECK(scene.IsAlive(scene.Resolve(id)));
    CHECK(scene.AliveCount() == 1);

    CHECK(stack.undo(scene).ok);
    CHECK_FALSE(scene.Resolve(id).IsValid());
    CHECK(scene.AliveCount() == 0);

    CHECK(stack.redo(scene).ok);
    CHECK(scene.IsAlive(scene.Resolve(id)));     // 同一逻辑身份回来
    CHECK(raw->CreatedId() == id);                // id 稳定
    CHECK(scene.AliveCount() == 1);
}
