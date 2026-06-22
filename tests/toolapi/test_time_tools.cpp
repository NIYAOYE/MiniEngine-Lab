#include <doctest/doctest.h>

#include "me/command/CommandStack.h"
#include "me/domain/TimeSystem.h"
#include "me/scene/Scene.h"
#include "me/toolapi/ToolContext.h"
#include "me/toolapi/ToolRegistry.h"
#include "me/toolapi/tools/BuiltinTools.h"

using namespace me::toolapi;

namespace {
me::domain::TimeConfig SmallConfig() {
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
} // namespace

TEST_CASE("TimeTools:time.get 返回当前日历") {
    ToolRegistry reg;
    reg.Register(MakeTimeGetTool());
    me::scene::Scene scene;
    me::command::CommandStack stack;
    ToolInvocationLog log;
    me::domain::TimeSystem ts(SmallConfig());
    ToolContext ctx{scene, stack, log, &ts};

    auto r = reg.Invoke("time.get", nlohmann::json::object(), CallerRole::Agent, ctx);
    REQUIRE(r.ok);
    CHECK(r.data["year"] == 1);
    CHECK(r.data["season"] == 0);
    CHECK(r.data["seasonName"] == "Spring");
    CHECK(r.data["dayOfSeason"] == 1);
    CHECK(r.data["minuteOfDay"] == 0);
}

TEST_CASE("TimeTools:time.get 无时间系统 PreconditionFailed") {
    ToolRegistry reg;
    reg.Register(MakeTimeGetTool());
    me::scene::Scene scene;
    me::command::CommandStack stack;
    ToolInvocationLog log;
    ToolContext ctx{scene, stack, log}; // time 默认 nullptr

    auto r = reg.Invoke("time.get", nlohmann::json::object(), CallerRole::Agent, ctx);
    CHECK_FALSE(r.ok);
    CHECK(r.code == ToolErrorCode::PreconditionFailed);
}
