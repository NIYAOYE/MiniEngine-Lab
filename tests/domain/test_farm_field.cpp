#include <doctest/doctest.h>

#include "me/domain/FarmField.h"

using namespace me::domain;

namespace {
// 两株作物:parsnip 4 阶段各 1 天;cauliflower 用于跨阶段测试。
CropDatabase MakeDb() {
    auto db = LoadCropDatabase(nlohmann::json::array({
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
         {"yield", 3}},
    }));
    REQUIRE(db.has_value());
    return *db;
}
} // namespace

TEST_CASE("FarmField:种植成功放入种子阶段") {
    FarmField field(MakeDb());
    CHECK(field.Plant(2, 3, "parsnip") == PlantStatus::Ok);
    const auto* c = field.At(2, 3);
    REQUIRE(c != nullptr);
    CHECK(c->cropId == "parsnip");
    CHECK(c->stage == 0);
    CHECK(c->daysInStage == 0);
    CHECK_FALSE(c->watered);
}

TEST_CASE("FarmField:重复种植同瓦片失败") {
    FarmField field(MakeDb());
    REQUIRE(field.Plant(2, 3, "parsnip") == PlantStatus::Ok);
    CHECK(field.Plant(2, 3, "cauliflower") == PlantStatus::TileOccupied);
}

TEST_CASE("FarmField:种植未知作物失败") {
    FarmField field(MakeDb());
    CHECK(field.Plant(0, 0, "banana") == PlantStatus::UnknownCrop);
    CHECK(field.At(0, 0) == nullptr);
}

TEST_CASE("FarmField:浇水标记") {
    FarmField field(MakeDb());
    REQUIRE(field.Plant(1, 1, "parsnip") == PlantStatus::Ok);
    CHECK(field.Water(1, 1));
    CHECK(field.At(1, 1)->watered);
    CHECK(field.Water(1, 1)); // 幂等
    CHECK(field.At(1, 1)->watered);
}

TEST_CASE("FarmField:浇空瓦片失败") {
    FarmField field(MakeDb());
    CHECK_FALSE(field.Water(5, 5));
}

TEST_CASE("FarmField:At 空瓦片返回 nullptr") {
    FarmField field(MakeDb());
    CHECK(field.At(9, 9) == nullptr);
}
