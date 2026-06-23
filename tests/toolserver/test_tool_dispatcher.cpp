#include <doctest/doctest.h>

#include <nlohmann/json.hpp>

#include "me/command/CommandStack.h"
#include "me/domain/CropConfig.h"
#include "me/domain/FarmField.h"
#include "me/domain/TimeConfig.h"
#include "me/domain/TimeSystem.h"
#include "me/scene/Scene.h"
#include "me/toolapi/ToolContext.h"
#include "me/toolapi/ToolRegistry.h"
#include "me/toolapi/tools/BuiltinTools.h"
#include "me/toolserver/ToolDispatcher.h"

using nlohmann::json;

namespace {

me::domain::TimeConfig MakeTimeConfig() {
    me::domain::TimeConfig c;
    c.minutesPerDay = 100;
    c.daysPerSeason = 3;
    c.seasonsPerYear = 2;
    c.seasonNames = {"Spring", "Summer"};
    c.gameMinutesPerStep = 10;
    c.realSecondsPerStep = 1.0;
    c.startYear = 1;
    c.startSeason = 0;
    c.startDay = 1;
    c.startMinute = 0;
    return c;
}

me::domain::CropDatabase MakeCropDb() {
    auto db = me::domain::LoadCropDatabase(json::array({
        {{"id", "parsnip"},
         {"name", "Parsnip"},
         {"stageNames", {"seed", "sprout", "growing", "mature"}},
         {"stageDays", {1, 1, 1, 1}},
         {"harvestItemId", "parsnip"},
         {"yield", 1}},
    }));
    REQUIRE(db.has_value());
    return *db;
}

/// @brief 拥有全部引擎状态 + 13 Tool 的可复用 dispatcher 夹具。
struct Fixture {
    me::scene::Scene scene;
    me::command::CommandStack stack;
    me::toolapi::ToolInvocationLog log;
    me::domain::TimeSystem time;
    me::domain::FarmField farm;
    me::toolapi::ToolRegistry registry;
    me::toolapi::ToolContext ctx;
    me::toolserver::ToolDispatcher dispatcher;

    Fixture()
        : time(MakeTimeConfig()),
          farm(MakeCropDb()),
          ctx{scene, stack, log, &time, &farm},
          dispatcher(ctx, registry) {
        me::toolapi::RegisterBuiltinTools(registry);
    }
};

} // namespace

TEST_CASE("ToolDispatcher:合法 invoke 创建实体并可见") {
    Fixture f;

    const json out = json::parse(f.dispatcher.HandleInvoke(R"({"name":"scene.create_entity"})"));
    REQUIRE(out["ok"] == true);
    CHECK(out["code"] == "Ok");
    CHECK(out["invocationId"].get<std::uint64_t>() > 0);

    const json listed = json::parse(f.dispatcher.HandleInvoke(R"({"name":"scene.list_entities"})"));
    REQUIRE(listed["ok"] == true);
    CHECK(listed["data"]["entities"].size() == 1);
}

TEST_CASE("ToolDispatcher:dryRun 零副作用") {
    Fixture f;

    const json dry = json::parse(
        f.dispatcher.HandleInvoke(R"({"name":"scene.create_entity","dryRun":true})"));
    REQUIRE(dry["ok"] == true);

    const json listed = json::parse(f.dispatcher.HandleInvoke(R"({"name":"scene.list_entities"})"));
    REQUIRE(listed["ok"] == true);
    CHECK(listed["data"]["entities"].size() == 0);
}

TEST_CASE("ToolDispatcher:body 非法 JSON → InvalidParams") {
    Fixture f;
    const json out = json::parse(f.dispatcher.HandleInvoke("{not json"));
    CHECK(out["ok"] == false);
    CHECK(out["code"] == "InvalidParams");
}

TEST_CASE("ToolDispatcher:body 非对象 → InvalidParams") {
    Fixture f;
    const json out = json::parse(f.dispatcher.HandleInvoke("[1,2,3]"));
    CHECK(out["ok"] == false);
    CHECK(out["code"] == "InvalidParams");
}

TEST_CASE("ToolDispatcher:缺 name → InvalidParams") {
    Fixture f;
    const json out = json::parse(f.dispatcher.HandleInvoke(R"({"params":{}})"));
    CHECK(out["ok"] == false);
    CHECK(out["code"] == "InvalidParams");
}

TEST_CASE("ToolDispatcher:未知 Tool → UnknownTool") {
    Fixture f;
    const json out = json::parse(f.dispatcher.HandleInvoke(R"({"name":"scene.no_such_tool"})"));
    CHECK(out["ok"] == false);
    CHECK(out["code"] == "UnknownTool");
}

TEST_CASE("ToolDispatcher:非法 role 字符串 → InvalidParams") {
    Fixture f;
    const json out = json::parse(
        f.dispatcher.HandleInvoke(R"({"name":"scene.list_entities","role":"Wizard"})"));
    CHECK(out["ok"] == false);
    CHECK(out["code"] == "InvalidParams");
}

// 注:destroy_entity 的 schema 使用 "id"(非 "entityId");
// PermissionDenied 在 schema 校验之前裁决,故 Agent 调用时无论参数是否存在都先返回权限错误。
TEST_CASE("ToolDispatcher:Agent 调 destroy(EditorOnly)→ PermissionDenied") {
    Fixture f;
    const json out = json::parse(f.dispatcher.HandleInvoke(
        R"({"name":"scene.destroy_entity","params":{"id":1},"role":"Agent"})"));
    CHECK(out["ok"] == false);
    CHECK(out["code"] == "PermissionDenied");
}

TEST_CASE("ToolDispatcher:destroy 不存在实体 → PreconditionFailed") {
    Fixture f;
    const json out = json::parse(f.dispatcher.HandleInvoke(
        R"({"name":"scene.destroy_entity","params":{"id":999},"role":"Editor"})"));
    CHECK(out["ok"] == false);
    CHECK(out["code"] == "PreconditionFailed");
}

TEST_CASE("ToolDispatcher:role 裁决 — Agent 调 time.advance(Automation)被拒") {
    Fixture f;
    const json out = json::parse(f.dispatcher.HandleInvoke(
        R"({"name":"time.advance","params":{"minutes":10},"role":"Agent"})"));
    CHECK(out["ok"] == false);
    CHECK(out["code"] == "PermissionDenied");
}

TEST_CASE("ToolDispatcher:role 裁决 — Automation 调 time.advance 放行") {
    Fixture f;
    const json out = json::parse(f.dispatcher.HandleInvoke(
        R"({"name":"time.advance","params":{"minutes":10},"role":"Automation"})"));
    CHECK(out["ok"] == true);
}

TEST_CASE("ToolDispatcher:role 缺省为 Editor — time.advance 放行") {
    Fixture f;
    const json out = json::parse(
        f.dispatcher.HandleInvoke(R"({"name":"time.advance","params":{"minutes":10}})"));
    CHECK(out["ok"] == true);
}
