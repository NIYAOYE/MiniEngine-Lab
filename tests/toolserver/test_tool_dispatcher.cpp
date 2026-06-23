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
