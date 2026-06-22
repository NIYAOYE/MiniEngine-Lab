#include <doctest/doctest.h>

#include "me/domain/TimeConfig.h"

using me::domain::LoadTimeConfig;

namespace {
// 一份合法的最小配置(星露谷式取值,仅测试内用)。
nlohmann::json ValidJson() {
    return nlohmann::json{
        {"minutesPerDay", 1440},
        {"daysPerSeason", 28},
        {"seasonsPerYear", 4},
        {"seasonNames", {"Spring", "Summer", "Fall", "Winter"}},
        {"gameMinutesPerStep", 10},
        {"realSecondsPerStep", 7.0},
        {"startYear", 1},
        {"startSeason", 0},
        {"startDay", 1},
        {"startMinute", 360}, // 06:00
    };
}
} // namespace

TEST_CASE("TimeConfig:合法 JSON 全字段解析") {
    auto c = LoadTimeConfig(ValidJson());
    REQUIRE(c.has_value());
    CHECK(c->minutesPerDay == 1440);
    CHECK(c->daysPerSeason == 28);
    CHECK(c->seasonsPerYear == 4);
    CHECK(c->gameMinutesPerStep == 10);
    CHECK(c->realSecondsPerStep == doctest::Approx(7.0));
    CHECK(c->startYear == 1);
    CHECK(c->startSeason == 0);
    CHECK(c->startDay == 1);
    CHECK(c->startMinute == 360);
    REQUIRE(c->seasonNames.size() == 4);
    CHECK(c->seasonNames[2] == "Fall");
}

TEST_CASE("TimeConfig:非法配置一律 nullopt") {
    SUBCASE("缺字段") {
        auto j = ValidJson();
        j.erase("minutesPerDay");
        CHECK_FALSE(LoadTimeConfig(j).has_value());
    }
    SUBCASE("缺 realSecondsPerStep(非 ReadInt 路径)") {
        auto j = ValidJson();
        j.erase("realSecondsPerStep");
        CHECK_FALSE(LoadTimeConfig(j).has_value());
    }
    SUBCASE("缺 seasonNames(非 ReadInt 路径)") {
        auto j = ValidJson();
        j.erase("seasonNames");
        CHECK_FALSE(LoadTimeConfig(j).has_value());
    }
    SUBCASE("数值非正") {
        auto j = ValidJson();
        j["daysPerSeason"] = 0;
        CHECK_FALSE(LoadTimeConfig(j).has_value());
    }
    SUBCASE("seasonNames 数量不符 seasonsPerYear") {
        auto j = ValidJson();
        j["seasonNames"] = {"Spring", "Summer"};
        CHECK_FALSE(LoadTimeConfig(j).has_value());
    }
    SUBCASE("startMinute 越界 minutesPerDay") {
        auto j = ValidJson();
        j["startMinute"] = 1440;
        CHECK_FALSE(LoadTimeConfig(j).has_value());
    }
    SUBCASE("startSeason 越界") {
        auto j = ValidJson();
        j["startSeason"] = 4;
        CHECK_FALSE(LoadTimeConfig(j).has_value());
    }
    SUBCASE("startDay < 1") {
        auto j = ValidJson();
        j["startDay"] = 0;
        CHECK_FALSE(LoadTimeConfig(j).has_value());
    }
    SUBCASE("非对象") {
        CHECK_FALSE(LoadTimeConfig(nlohmann::json::array()).has_value());
    }
}
