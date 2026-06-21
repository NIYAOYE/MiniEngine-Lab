#include <doctest/doctest.h>

#include "me/command/CommandStack.h"
#include "me/scene/Scene.h"
#include "me/toolapi/ToolContext.h"
#include "me/toolapi/ToolRegistry.h"
#include "me/toolapi/tools/BuiltinTools.h"

using namespace me::toolapi;

namespace {
ToolRegistry MakeReg() {
    ToolRegistry reg;
    reg.Register(MakeListEntitiesTool());
    reg.Register(MakeGetEntityTool());
    return reg;
}
} // namespace

TEST_CASE("QueryTools:list_entities 返回全部存活实体") {
    ToolRegistry reg = MakeReg();
    me::scene::Scene scene;
    me::command::CommandStack stack;
    ToolInvocationLog log;
    ToolContext ctx{scene, stack, log};

    me::scene::Entity e0 = scene.CreateEntity();
    scene.SetLocalTransform(e0, me::Transform2D{{2.0f, 3.0f}, 0.0f, {1.0f, 1.0f}});
    scene.CreateEntity();

    auto r = reg.Invoke("scene.list_entities", nlohmann::json::object(),
                        CallerRole::Agent, ctx);
    CHECK(r.ok);
    CHECK(r.data["count"] == 2);
    CHECK(r.data["entities"].size() == 2);
}

TEST_CASE("QueryTools:get_entity 命中返回变换,缺失 PreconditionFailed") {
    ToolRegistry reg = MakeReg();
    me::scene::Scene scene;
    me::command::CommandStack stack;
    ToolInvocationLog log;
    ToolContext ctx{scene, stack, log};

    me::scene::Entity e = scene.CreateEntity();
    scene.SetLocalTransform(e, me::Transform2D{{5.0f, 7.0f}, 0.0f, {1.0f, 1.0f}});
    const auto id = scene.IdOf(e);

    auto hit = reg.Invoke("scene.get_entity", {{"id", id}}, CallerRole::Agent, ctx);
    CHECK(hit.ok);
    CHECK(hit.data["id"] == id);
    CHECK(hit.data["position"]["x"] == doctest::Approx(5.0f));
    CHECK(hit.data["position"]["y"] == doctest::Approx(7.0f));

    auto miss = reg.Invoke("scene.get_entity", {{"id", 9999}}, CallerRole::Agent, ctx);
    CHECK(miss.code == ToolErrorCode::PreconditionFailed);
}
