#include <doctest/doctest.h>

#include "me/domain/CropConfig.h"

using me::domain::LoadCropDatabase;

namespace {
// 一份合法作物表(与 assets/config/crops.json 取值严格一致;仅测试内用)。
nlohmann::json ValidCropJson() {
    return nlohmann::json::array({
        {{"id", "parsnip"},
         {"name", "Parsnip"},
         {"stageNames", {"seed", "sprout", "growing", "mature"}},
         {"stageDays", {1, 1, 1, 1}},
         {"harvestItemId", "parsnip"},
         {"yield", 1}},
        {{"id", "cauliflower"},
         {"name", "Cauliflower"},
         {"stageNames", {"seed", "sprout", "leafy", "heading", "mature"}},
         {"stageDays", {2, 2, 2, 2, 2}},
         {"harvestItemId", "cauliflower"},
         {"yield", 1}},
    });
}
} // namespace

TEST_CASE("CropConfig:合法作物表全字段解析") {
    auto db = LoadCropDatabase(ValidCropJson());
    REQUIRE(db.has_value());
    CHECK(db->Size() == 2);

    const auto* p = db->Find("parsnip");
    REQUIRE(p != nullptr);
    CHECK(p->name == "Parsnip");
    CHECK(p->StageCount() == 4);
    CHECK(p->LastStage() == 3);
    CHECK(p->stageDays[0] == 1);
    CHECK(p->harvestItemId == "parsnip");
    CHECK(p->yield == 1);
    CHECK(p->IsMatureStage(3));
    CHECK_FALSE(p->IsMatureStage(2));

    CHECK(db->Find("cauliflower") != nullptr);
    CHECK(db->Find("nonexistent") == nullptr);
}

TEST_CASE("CropConfig:非法配置返回 nullopt") {
    SUBCASE("顶层非数组") {
        CHECK_FALSE(LoadCropDatabase(nlohmann::json::object()).has_value());
    }
    SUBCASE("stageNames 与 stageDays 长度不匹配") {
        auto j = ValidCropJson();
        j[0]["stageDays"] = {1, 1, 1}; // 4 名 vs 3 天
        CHECK_FALSE(LoadCropDatabase(j).has_value());
    }
    SUBCASE("stageDays 含 <1") {
        auto j = ValidCropJson();
        j[0]["stageDays"] = {1, 0, 1, 1};
        CHECK_FALSE(LoadCropDatabase(j).has_value());
    }
    SUBCASE("yield < 1") {
        auto j = ValidCropJson();
        j[0]["yield"] = 0;
        CHECK_FALSE(LoadCropDatabase(j).has_value());
    }
    SUBCASE("空 id") {
        auto j = ValidCropJson();
        j[0]["id"] = "";
        CHECK_FALSE(LoadCropDatabase(j).has_value());
    }
    SUBCASE("id 重复") {
        auto j = ValidCropJson();
        j[1]["id"] = "parsnip";
        CHECK_FALSE(LoadCropDatabase(j).has_value());
    }
    SUBCASE("缺 harvestItemId") {
        auto j = ValidCropJson();
        j[0].erase("harvestItemId");
        CHECK_FALSE(LoadCropDatabase(j).has_value());
    }
    SUBCASE("stageNames 空") {
        auto j = ValidCropJson();
        j[0]["stageNames"] = nlohmann::json::array();
        j[0]["stageDays"] = nlohmann::json::array();
        CHECK_FALSE(LoadCropDatabase(j).has_value());
    }
    SUBCASE("yield 类型错(字符串)") {
        auto j = ValidCropJson();
        j[0]["yield"] = "1";
        CHECK_FALSE(LoadCropDatabase(j).has_value());
    }
}
