#include <doctest/doctest.h>

#include "me/domain/TimeSystem.h"

using namespace me::domain;

namespace {
// 简洁取值,便于手算边界:一天 100 分钟、一季 3 天、一年 2 季。
TimeConfig SmallConfig() {
    TimeConfig c;
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

TEST_CASE("TimeSystem:起点 Now() 派生") {
    TimeSystem ts(SmallConfig());
    auto n = ts.Now();
    CHECK(n.year == 1);
    CHECK(n.season == 0);
    CHECK(n.seasonName == "Spring");
    CHECK(n.dayOfSeason == 1);
    CHECK(n.minuteOfDay == 0);
    CHECK(n.hour == 0);
    CHECK(n.minute == 0);
}

TEST_CASE("TimeSystem:Now() hour/minute 由 minuteOfDay 派生") {
    auto cfg = SmallConfig();
    cfg.minutesPerDay = 1440;
    cfg.startMinute = 125; // 02:05
    TimeSystem ts(cfg);
    auto n = ts.Now();
    CHECK(n.minuteOfDay == 125);
    CHECK(n.hour == 2);
    CHECK(n.minute == 5);
}

TEST_CASE("TimeSystem:Advance 单分钟不跨界") {
    TimeSystem ts(SmallConfig());
    auto s = ts.Advance(1);
    CHECK(s.minutesAdvanced == 1);
    CHECK(s.daysCrossed == 0);
    CHECK(s.seasonsCrossed == 0);
    CHECK(s.yearsCrossed == 0);
    CHECK(ts.Now().minuteOfDay == 1);
}

TEST_CASE("TimeSystem:Advance 跨天") {
    TimeSystem ts(SmallConfig()); // 一天 100 分钟
    auto s = ts.Advance(100);
    CHECK(s.daysCrossed == 1);
    CHECK(s.seasonsCrossed == 0);
    auto n = ts.Now();
    CHECK(n.dayOfSeason == 2);
    CHECK(n.minuteOfDay == 0);
}

TEST_CASE("TimeSystem:Advance 跨季") {
    TimeSystem ts(SmallConfig()); // 一季 = 3*100 = 300 分钟
    auto s = ts.Advance(300);
    CHECK(s.daysCrossed == 3);
    CHECK(s.seasonsCrossed == 1);
    auto n = ts.Now();
    CHECK(n.season == 1);
    CHECK(n.seasonName == "Summer");
    CHECK(n.dayOfSeason == 1);
}

TEST_CASE("TimeSystem:Advance 跨年") {
    TimeSystem ts(SmallConfig()); // 一年 = 2*300 = 600 分钟
    auto s = ts.Advance(600);
    CHECK(s.yearsCrossed == 1);
    CHECK(s.seasonsCrossed == 2);
    auto n = ts.Now();
    CHECK(n.year == 2);
    CHECK(n.season == 0);
    CHECK(n.dayOfSeason == 1);
}

TEST_CASE("TimeSystem:Advance 一次跨多天计数正确") {
    TimeSystem ts(SmallConfig());
    auto s = ts.Advance(250); // 2.5 天
    CHECK(s.daysCrossed == 2);
    CHECK(ts.Now().dayOfSeason == 3);
    CHECK(ts.Now().minuteOfDay == 50);
}

TEST_CASE("TimeSystem:Advance 从非零起点正确累加跨界") {
    auto cfg = SmallConfig();
    cfg.startMinute = 50; // 距跨天还差 50 分钟
    TimeSystem ts(cfg);
    auto s = ts.Advance(60);
    CHECK(s.daysCrossed == 1); // 50→110 跨过 100 边界一次
}
