#include <doctest/doctest.h>

#include "me/domain/Inventory.h"

using namespace me::domain;

namespace {
// 物品表:parsnip(maxStack 5,便于触发溢出),stone(maxStack 99)。
ItemDatabase MakeItemDb() {
    auto cfg = LoadInventoryConfig(nlohmann::json{
        {"capacity", 3},
        {"items",
         nlohmann::json::array(
             {{{"id", "parsnip"}, {"name", "Parsnip"}, {"category", "crop"},
               {"maxStack", 5}, {"sellPrice", 35}},
              {{"id", "stone"}, {"name", "Stone"}, {"category", "resource"},
               {"maxStack", 99}, {"sellPrice", 2}}})}});
    REQUIRE(cfg.has_value());
    return cfg->items;
}
} // namespace

TEST_CASE("Inventory:Add 先补未满堆再占空格") {
    Inventory inv(MakeItemDb(), 3);
    CHECK(inv.Add("parsnip", 3).status == AddStatus::Ok);
    CHECK(inv.CountOf("parsnip") == 3);
    // 再加 4:先把首堆补到 5(+2),余 2 占新空格。
    auto r = inv.Add("parsnip", 4);
    CHECK(r.status == AddStatus::Ok);
    CHECK(r.added == 4);
    CHECK(inv.CountOf("parsnip") == 7);
    CHECK(inv.Slots()[0].count == 5);
    CHECK(inv.Slots()[1].count == 2);
}

TEST_CASE("Inventory:Add 未知物品 UnknownItem 零变更") {
    Inventory inv(MakeItemDb(), 3);
    auto r = inv.Add("banana", 1);
    CHECK(r.status == AddStatus::UnknownItem);
    CHECK(inv.CountOf("banana") == 0);
}

TEST_CASE("Inventory:Add 放不下 Full 零变更") {
    Inventory inv(MakeItemDb(), 1);   // 仅 1 格,maxStack 5
    REQUIRE(inv.Add("parsnip", 5).status == AddStatus::Ok);
    auto r = inv.Add("parsnip", 1);   // 已满
    CHECK(r.status == AddStatus::Full);
    CHECK(r.added == 0);
    CHECK(inv.CountOf("parsnip") == 5); // 真身未变
}

TEST_CASE("Inventory:CanAdd 与 Add 一致") {
    Inventory inv(MakeItemDb(), 1);
    CHECK(inv.CanAdd("parsnip", 5));
    CHECK_FALSE(inv.CanAdd("parsnip", 6));
    CHECK_FALSE(inv.CanAdd("banana", 1)); // 未知物品
}

TEST_CASE("Inventory:Remove 不足 NotEnough 零变更") {
    Inventory inv(MakeItemDb(), 3);
    REQUIRE(inv.Add("parsnip", 2).status == AddStatus::Ok);
    auto r = inv.Remove("parsnip", 5);
    CHECK(r.status == RemoveStatus::NotEnough);
    CHECK(inv.CountOf("parsnip") == 2); // 未变
}

TEST_CASE("Inventory:Remove 跨格扣减并清空被掏空格") {
    Inventory inv(MakeItemDb(), 3);
    REQUIRE(inv.Add("parsnip", 7).status == AddStatus::Ok); // 堆0=5, 堆1=2
    auto r = inv.Remove("parsnip", 6);
    CHECK(r.status == RemoveStatus::Ok);
    CHECK(r.removed == 6);
    CHECK(inv.CountOf("parsnip") == 1);
    // 首格被掏空后应清空 itemId。
    int nonEmpty = 0;
    for (const auto& s : inv.Slots()) if (!s.Empty()) ++nonEmpty;
    CHECK(nonEmpty == 1);
}

TEST_CASE("Inventory:值拷贝独立") {
    Inventory inv(MakeItemDb(), 3);
    REQUIRE(inv.Add("parsnip", 2).status == AddStatus::Ok);
    Inventory copy = inv;
    copy.Add("parsnip", 3);
    CHECK(inv.CountOf("parsnip") == 2);   // 真身不受副本影响
    CHECK(copy.CountOf("parsnip") == 5);
}
