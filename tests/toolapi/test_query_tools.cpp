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
    // 验证 list 路径经 TransformToJson 正确序列化变换字段(e0 槽位序在前)
    const auto e0Id = scene.IdOf(e0);
    bool found = false;
    for (const auto& item : r.data["entities"]) {
        if (item["id"] == e0Id) {
            CHECK(item["position"]["x"] == doctest::Approx(2.0f));
            CHECK(item["position"]["y"] == doctest::Approx(3.0f));
            found = true;
        }
    }
    CHECK(found);
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

TEST_CASE("QueryTools:log.read 返回已记录调用,limit 截尾") {
    ToolRegistry reg;
    reg.Register(MakeListEntitiesTool());
    reg.Register(MakeLogReadTool());

    me::scene::Scene scene;
    me::command::CommandStack stack;
    ToolInvocationLog log;
    ToolContext ctx{scene, stack, log};

    // 制造若干调用记录
    reg.Invoke("scene.list_entities", nlohmann::json::object(), CallerRole::Agent, ctx);
    reg.Invoke("scene.list_entities", nlohmann::json::object(), CallerRole::Agent, ctx);

    auto all = reg.Invoke("log.read", nlohmann::json::object(), CallerRole::Agent, ctx);
    CHECK(all.ok);
    // 此前 2 次 list + 本次 log.read 的记录尚未在 run 内可见(record 在 run 之后)
    CHECK(all.data["count"] == 2);

    auto limited = reg.Invoke("log.read", {{"limit", 1}}, CallerRole::Agent, ctx);
    CHECK(limited.data["invocations"].size() == 1);
}
