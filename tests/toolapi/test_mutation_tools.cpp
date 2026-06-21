#include <doctest/doctest.h>

#include "me/command/CommandStack.h"
#include "me/scene/Scene.h"
#include "me/toolapi/ToolContext.h"
#include "me/toolapi/ToolRegistry.h"
#include "me/toolapi/tools/BuiltinTools.h"

using namespace me::toolapi;

namespace {
struct Fixture {
    ToolRegistry reg;
    me::scene::Scene scene;
    me::command::CommandStack stack;
    ToolInvocationLog log;
    ToolContext ctx{scene, stack, log};
    Fixture() {
        reg.Register(MakeCreateEntityTool());
        reg.Register(MakeDestroyEntityTool());
        reg.Register(MakeSetTransformTool());
    }
};
} // namespace

TEST_CASE("MutationTools:create_entity 落地 + dryRun 零副作用") {
    Fixture f;
    auto dry = f.reg.Invoke("scene.create_entity", nlohmann::json::object(),
                            CallerRole::Automation, f.ctx, /*dryRun=*/true);
    CHECK(dry.ok);
    CHECK(f.scene.AliveCount() == 0); // 预览无副作用

    auto r = f.reg.Invoke("scene.create_entity", nlohmann::json::object(),
                          CallerRole::Automation, f.ctx);
    CHECK(r.ok);
    CHECK(f.scene.AliveCount() == 1);
    const auto id = r.data["id"].get<me::scene::EntityId>();
    CHECK(f.scene.IsAlive(f.scene.Resolve(id)));

    CHECK(f.stack.undo(f.scene).ok); // 经 Command → 可撤销
    CHECK(f.scene.AliveCount() == 0);
}

TEST_CASE("MutationTools:destroy_entity EditorOnly + 缺失 PreconditionFailed") {
    Fixture f;
    me::scene::Entity e = f.scene.CreateEntity();
    const auto id = f.scene.IdOf(e);

    auto denied = f.reg.Invoke("scene.destroy_entity", {{"id", id}},
                               CallerRole::Automation, f.ctx);
    CHECK(denied.code == ToolErrorCode::PermissionDenied);
    CHECK(f.scene.AliveCount() == 1);

    auto miss = f.reg.Invoke("scene.destroy_entity", {{"id", 4242}},
                             CallerRole::Editor, f.ctx);
    CHECK(miss.code == ToolErrorCode::PreconditionFailed);

    auto ok = f.reg.Invoke("scene.destroy_entity", {{"id", id}}, CallerRole::Editor, f.ctx);
    CHECK(ok.ok);
    CHECK(f.scene.AliveCount() == 0);
}

TEST_CASE("MutationTools:set_transform 覆盖给定字段并可撤销") {
    Fixture f;
    me::scene::Entity e = f.scene.CreateEntity();
    f.scene.SetLocalTransform(e, me::Transform2D{{1.0f, 1.0f}, 0.0f, {1.0f, 1.0f}});
    const auto id = f.scene.IdOf(e);

    auto r = f.reg.Invoke("entity.set_transform",
                          {{"id", id}, {"position", {{"x", 8.0}, {"y", 9.0}}}},
                          CallerRole::Automation, f.ctx);
    CHECK(r.ok);
    CHECK(f.scene.LocalTransform(e).position.x == doctest::Approx(8.0f));
    CHECK(f.scene.LocalTransform(e).position.y == doctest::Approx(9.0f));
    CHECK(f.scene.LocalTransform(e).scale.x == doctest::Approx(1.0f)); // 未给字段保持

    CHECK(f.stack.undo(f.scene).ok);
    CHECK(f.scene.LocalTransform(e).position.x == doctest::Approx(1.0f)); // 还原
}
