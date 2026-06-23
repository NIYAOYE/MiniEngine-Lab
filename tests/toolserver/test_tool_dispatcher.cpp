#include <doctest/doctest.h>

#include <nlohmann/json.hpp>

#include "me/command/CommandStack.h"
#include "me/domain/CropConfig.h"
#include "me/domain/FarmField.h"
#include "me/domain/Inventory.h"
#include "me/domain/ItemConfig.h"
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

me::domain::InventoryConfig MakeInvDb() {
    auto cfg = me::domain::LoadInventoryConfig(nlohmann::json{
        {"capacity", 8},
        {"items",
         nlohmann::json::array(
             {{{"id", "parsnip"}, {"name", "Parsnip"}, {"category", "crop"},
               {"maxStack", 99}, {"sellPrice", 35}}})}});
    REQUIRE(cfg.has_value());
    return *cfg;
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

/// @brief 拥有全部引擎状态 + 16 Tool 的可复用 dispatcher 夹具。
struct Fixture {
    me::scene::Scene scene;
    me::command::CommandStack stack;
    me::toolapi::ToolInvocationLog log;
    me::domain::TimeSystem time;
    me::domain::FarmField farm;
    me::domain::Inventory inventory;
    me::toolapi::ToolRegistry registry;
    me::toolapi::ToolContext ctx;
    me::toolserver::ToolDispatcher dispatcher;

    Fixture()
        : time(MakeTimeConfig()),
          farm(MakeCropDb()),
          inventory([]() { auto c = MakeInvDb(); return me::domain::Inventory(c.items, c.capacity); }()),
          ctx{scene, stack, log, &time, &farm, &inventory},
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

TEST_CASE("ToolDispatcher:HandleListTools 返回 16 条带元数据") {
    Fixture f;
    const json tools = json::parse(f.dispatcher.HandleListTools());
    REQUIRE(tools.is_array());
    CHECK(tools.size() == 16);

    // 收集成 name → 条目,便于断言具体 Tool。
    json byName = json::object();
    for (const auto& t : tools) {
        REQUIRE(t.contains("name"));
        REQUIRE(t.contains("category"));
        REQUIRE(t.contains("permission"));
        REQUIRE(t.contains("paramsSchema"));
        byName[t["name"].get<std::string>()] = t;
    }

    REQUIRE(byName.contains("entity.set_transform"));
    CHECK(byName["entity.set_transform"]["category"] == "Mutation");
    CHECK(byName["entity.set_transform"]["permission"] == "Automation");
    // paramsSchema 是 JSON Schema 子集对象,含 required 列表。
    CHECK(byName["entity.set_transform"]["paramsSchema"]["type"] == "object");

    REQUIRE(byName.contains("scene.list_entities"));
    CHECK(byName["scene.list_entities"]["category"] == "Query");
    CHECK(byName["scene.list_entities"]["permission"] == "AgentAllowed");

    REQUIRE(byName.contains("scene.destroy_entity"));
    CHECK(byName["scene.destroy_entity"]["permission"] == "EditorOnly");
}

TEST_CASE("ToolDispatcher:time.advance 推进后 time.get 反映变化") {
    Fixture f;
    const json before = json::parse(f.dispatcher.HandleInvoke(R"({"name":"time.get"})"));
    REQUIRE(before["ok"] == true);
    const int beforeMinute = before["data"]["minuteOfDay"].get<int>();

    const json adv = json::parse(
        f.dispatcher.HandleInvoke(R"({"name":"time.advance","params":{"minutes":10}})"));
    REQUIRE(adv["ok"] == true);

    const json after = json::parse(f.dispatcher.HandleInvoke(R"({"name":"time.get"})"));
    REQUIRE(after["ok"] == true);
    CHECK(after["data"]["minuteOfDay"].get<int>() != beforeMinute);
}

TEST_CASE("ToolDispatcher:crop.plant 后 crop.get_field 可见") {
    Fixture f;
    const json plant = json::parse(f.dispatcher.HandleInvoke(
        R"({"name":"crop.plant","params":{"tileX":2,"tileY":3,"cropId":"parsnip"}})"));
    REQUIRE(plant["ok"] == true);

    const json field = json::parse(f.dispatcher.HandleInvoke(R"({"name":"crop.get_field"})"));
    REQUIRE(field["ok"] == true);
    REQUIRE(field["data"]["crops"].size() == 1);
    CHECK(field["data"]["crops"][0]["cropId"] == "parsnip");
}

TEST_CASE("ToolDispatcher:inventory.add 后 inventory.get 反映库存") {
    Fixture f;

    // Automation 角色通过 dispatcher 字符串路径加入 3 个 parsnip。
    const json add = json::parse(f.dispatcher.HandleInvoke(
        R"({"name":"inventory.add","params":{"itemId":"parsnip","count":3},"role":"Automation"})"));
    REQUIRE(add["ok"] == true);
    CHECK(add["code"] == "Ok");

    // Agent 角色查询库存,确认格位与数量。
    const json get = json::parse(f.dispatcher.HandleInvoke(
        R"({"name":"inventory.get","role":"Agent"})"));
    REQUIRE(get["ok"] == true);
    CHECK(get["data"]["used"] == 1);
    CHECK(get["data"]["slots"][0]["itemId"] == "parsnip");
    CHECK(get["data"]["slots"][0]["count"] == 3);
}
