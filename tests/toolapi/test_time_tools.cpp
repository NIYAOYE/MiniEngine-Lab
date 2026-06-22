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

TEST_CASE("TimeTools:time.advance 推进改变当前时间") {
    ToolRegistry reg;
    reg.Register(MakeTimeAdvanceTool());
    me::scene::Scene scene;
    me::command::CommandStack stack;
    ToolInvocationLog log;
    me::domain::TimeSystem ts(SmallConfig()); // 一天 100 分钟
    ToolContext ctx{scene, stack, log, &ts};

    auto r = reg.Invoke("time.advance", {{"minutes", 120}}, CallerRole::Automation, ctx);
    REQUIRE(r.ok);
    CHECK(r.data["step"]["minutesAdvanced"] == 120);
    CHECK(r.data["step"]["daysCrossed"] == 1);
    CHECK(r.data["time"]["dayOfSeason"] == 2);
    CHECK(r.data["time"]["minuteOfDay"] == 20);
    CHECK(ts.Now().minuteOfDay == 20); // 真实状态已推进
}

TEST_CASE("TimeTools:time.advance dry-run 零副作用") {
    ToolRegistry reg;
    reg.Register(MakeTimeAdvanceTool());
    me::scene::Scene scene;
    me::command::CommandStack stack;
    ToolInvocationLog log;
    me::domain::TimeSystem ts(SmallConfig());
    ToolContext ctx{scene, stack, log, &ts};

    auto r = reg.Invoke("time.advance", {{"minutes", 120}}, CallerRole::Automation, ctx,
                        /*dryRun=*/true);
    REQUIRE(r.ok);
    CHECK(r.data["step"]["daysCrossed"] == 1);     // 预览给出将会发生什么
    CHECK(ts.Now().minuteOfDay == 0);               // 真实状态未变
    CHECK(ts.Now().dayOfSeason == 1);
}

TEST_CASE("TimeTools:time.advance schema 拒绝") {
    ToolRegistry reg;
    reg.Register(MakeTimeAdvanceTool());
    me::scene::Scene scene;
    me::command::CommandStack stack;
    ToolInvocationLog log;
    me::domain::TimeSystem ts(SmallConfig());
    ToolContext ctx{scene, stack, log, &ts};

    SUBCASE("缺 minutes") {
        auto r = reg.Invoke("time.advance", nlohmann::json::object(),
                            CallerRole::Automation, ctx);
        CHECK_FALSE(r.ok);
        CHECK(r.code == ToolErrorCode::InvalidParams);
    }
    SUBCASE("minutes < 1") {
        auto r = reg.Invoke("time.advance", {{"minutes", 0}}, CallerRole::Automation, ctx);
        CHECK_FALSE(r.ok);
        CHECK(r.code == ToolErrorCode::InvalidParams);
    }
}

TEST_CASE("TimeTools:time.advance 无时间系统 PreconditionFailed") {
    ToolRegistry reg;
    reg.Register(MakeTimeAdvanceTool());
    me::scene::Scene scene;
    me::command::CommandStack stack;
    ToolInvocationLog log;
    ToolContext ctx{scene, stack, log}; // time = nullptr

    auto r = reg.Invoke("time.advance", {{"minutes", 10}}, CallerRole::Automation, ctx);
    CHECK_FALSE(r.ok);
    CHECK(r.code == ToolErrorCode::PreconditionFailed);
}

TEST_CASE("TimeTools:time.advance 权限——Agent 被拒") {
    ToolRegistry reg;
    reg.Register(MakeTimeAdvanceTool());
    me::scene::Scene scene;
    me::command::CommandStack stack;
    ToolInvocationLog log;
    me::domain::TimeSystem ts(SmallConfig());
    ToolContext ctx{scene, stack, log, &ts};

    auto r = reg.Invoke("time.advance", {{"minutes", 10}}, CallerRole::Agent, ctx);
    CHECK_FALSE(r.ok);
    CHECK(r.code == ToolErrorCode::PermissionDenied); // Automation 权限,Agent 不足
}
