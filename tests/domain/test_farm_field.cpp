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

TEST_CASE("FarmField:浇水后推进一天进入下一阶段") {
    FarmField field(MakeDb());
    REQUIRE(field.Plant(0, 0, "parsnip") == PlantStatus::Ok); // 各阶段 1 天
    REQUIRE(field.Water(0, 0));
    field.AdvanceDays(1);
    const auto* c = field.At(0, 0);
    REQUIRE(c != nullptr);
    CHECK(c->stage == 1);
    CHECK(c->daysInStage == 0);
    CHECK_FALSE(c->watered); // 推进后浇水标记清除
}

TEST_CASE("FarmField:未浇水推进停滞") {
    FarmField field(MakeDb());
    REQUIRE(field.Plant(0, 0, "parsnip") == PlantStatus::Ok);
    field.AdvanceDays(3); // 从未浇水
    const auto* c = field.At(0, 0);
    CHECK(c->stage == 0);
    CHECK(c->daysInStage == 0);
}

TEST_CASE("FarmField:多阶段作物逐阶段推进(每阶段 2 天)") {
    FarmField field(MakeDb());
    REQUIRE(field.Plant(0, 0, "cauliflower") == PlantStatus::Ok); // 5 阶段各 2 天
    // 第 1 天浇水推进:daysInStage 1,仍 stage 0
    REQUIRE(field.Water(0, 0));
    field.AdvanceDays(1);
    CHECK(field.At(0, 0)->stage == 0);
    CHECK(field.At(0, 0)->daysInStage == 1);
    // 第 2 天浇水推进:满 2 天 → stage 1
    REQUIRE(field.Water(0, 0));
    field.AdvanceDays(1);
    CHECK(field.At(0, 0)->stage == 1);
    CHECK(field.At(0, 0)->daysInStage == 0);
}

TEST_CASE("FarmField:浇水→推进循环可达成熟阶段") {
    FarmField field(MakeDb());
    REQUIRE(field.Plant(0, 0, "parsnip") == PlantStatus::Ok); // 4 阶段各 1 天
    for (int day = 0; day < 3; ++day) { // 3 次进阶到 stage 3(成熟)
        REQUIRE(field.Water(0, 0));
        field.AdvanceDays(1);
    }
    const auto* c = field.At(0, 0);
    CHECK(c->stage == 3);
    CHECK(c->cropId == "parsnip");
}

TEST_CASE("FarmField:成熟后继续浇水推进不再前进") {
    FarmField field(MakeDb());
    REQUIRE(field.Plant(0, 0, "parsnip") == PlantStatus::Ok);
    for (int day = 0; day < 3; ++day) {
        REQUIRE(field.Water(0, 0));
        field.AdvanceDays(1);
    }
    REQUIRE(field.At(0, 0)->stage == 3); // 成熟
    field.Water(0, 0);
    field.AdvanceDays(5);
    CHECK(field.At(0, 0)->stage == 3); // 仍停在成熟
}

namespace {
// 把瓦片上的作物推到成熟阶段(parsnip:3 次浇水+推进)。
void GrowParsnipToMature(FarmField& field, int x, int y) {
    REQUIRE(field.Plant(x, y, "parsnip") == PlantStatus::Ok);
    for (int day = 0; day < 3; ++day) {
        REQUIRE(field.Water(x, y));
        field.AdvanceDays(1);
    }
    REQUIRE(field.At(x, y)->stage == 3);
}
} // namespace

TEST_CASE("FarmField:成熟作物收获产出并清空瓦片") {
    FarmField field(MakeDb());
    GrowParsnipToMature(field, 4, 4);
    auto r = field.Harvest(4, 4);
    CHECK(r.status == HarvestStatus::Ok);
    CHECK(r.itemId == "parsnip");
    CHECK(r.count == 1);
    CHECK(field.At(4, 4) == nullptr); // 瓦片已清空
}

TEST_CASE("FarmField:产量取自配置(cauliflower yield=3)") {
    FarmField field(MakeDb());
    REQUIRE(field.Plant(0, 0, "cauliflower") == PlantStatus::Ok); // 5 阶段各 2 天
    for (int i = 0; i < 8; ++i) { // 4 次进阶 ×2 天 = 8 浇水日到 stage 4(成熟)
        REQUIRE(field.Water(0, 0));
        field.AdvanceDays(1);
    }
    REQUIRE(field.At(0, 0)->stage == 4);
    auto r = field.Harvest(0, 0);
    CHECK(r.status == HarvestStatus::Ok);
    CHECK(r.count == 3);
}

TEST_CASE("FarmField:未成熟收获失败且不清空") {
    FarmField field(MakeDb());
    REQUIRE(field.Plant(4, 4, "parsnip") == PlantStatus::Ok);
    auto r = field.Harvest(4, 4);
    CHECK(r.status == HarvestStatus::NotMature);
    CHECK(field.At(4, 4) != nullptr); // 仍在
}

TEST_CASE("FarmField:空瓦片收获返回 EmptyTile") {
    FarmField field(MakeDb());
    auto r = field.Harvest(7, 7);
    CHECK(r.status == HarvestStatus::EmptyTile);
}
