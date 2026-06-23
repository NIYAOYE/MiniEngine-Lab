#include <doctest/doctest.h>

#include "me/command/CommandStack.h"
#include "me/scene/Scene.h"
#include "me/toolapi/ToolContext.h"
#include "me/toolapi/ToolRegistry.h"
#include "me/toolapi/tools/BuiltinTools.h"

using namespace me::toolapi;

TEST_CASE("Integration:RegisterBuiltinTools 注册全部 16 个 Tool") {
    ToolRegistry reg;
    RegisterBuiltinTools(reg);
    CHECK(reg.Size() == 16);
    auto names = reg.ListNames();
    // 字典序:crop.* 排在最前(advance_days < get_field < harvest < plant < water),entity.* 次之
    CHECK(names[0] == "crop.advance_days");
    CHECK(names[1] == "crop.get_field");
    CHECK(names[2] == "crop.harvest");
    CHECK(names[3] == "crop.plant");
    CHECK(names[4] == "crop.water");
    CHECK(names[5] == "entity.set_transform");
}

TEST_CASE("Integration:create→set_transform→list→destroy→undo 全链路") {
    ToolRegistry reg;
    RegisterBuiltinTools(reg);
    me::scene::Scene scene;
    me::command::CommandStack stack;
    ToolInvocationLog log;
    ToolContext ctx{scene, stack, log};

    // create(Automation)
    auto c = reg.Invoke("scene.create_entity", nlohmann::json::object(),
                        CallerRole::Automation, ctx);
    REQUIRE(c.ok);
    const auto id = c.data["id"].get<me::scene::EntityId>();

    // set_transform(Automation)
    auto s = reg.Invoke("entity.set_transform",
                        {{"id", id}, {"position", {{"x", 4.0}, {"y", 6.0}}}},
                        CallerRole::Automation, ctx);
    CHECK(s.ok);

    // list_entities(Agent 只读)
    auto l = reg.Invoke("scene.list_entities", nlohmann::json::object(),
                        CallerRole::Agent, ctx);
    CHECK(l.data["count"] == 1);

    // destroy 被 Agent 拒绝、被 Editor 放行
    CHECK(reg.Invoke("scene.destroy_entity", {{"id", id}}, CallerRole::Agent, ctx).code
          == ToolErrorCode::PermissionDenied);
    CHECK(reg.Invoke("scene.destroy_entity", {{"id", id}}, CallerRole::Editor, ctx).ok);
    CHECK(scene.AliveCount() == 0);

    // undo 销毁 → 实体回来
    CHECK(stack.undo(scene).ok);
    CHECK(scene.AliveCount() == 1);

    // log.read 看到此前完整审计链:create + set_transform + list + destroy(拒绝)
    // + destroy(放行)= 5 次工具调用;undo 经 stack 直调非工具不入日志,
    // 且 log.read 在 run 后才记录自身,故此刻恰好 5 条。
    auto lr = reg.Invoke("log.read", nlohmann::json::object(), CallerRole::Agent, ctx);
    CHECK(lr.data["count"].get<std::size_t>() == 5);

    // 未知 Tool
    CHECK(reg.Invoke("scene.nope", {}, CallerRole::Editor, ctx).code
          == ToolErrorCode::UnknownTool);
}
