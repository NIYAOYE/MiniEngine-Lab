#include <doctest/doctest.h>

#include "me/domain/ItemConfig.h"

using me::domain::LoadInventoryConfig;

namespace {
// 一份合法库存配置(与 assets/config/items.json 取值严格一致;仅测试内用)。
nlohmann::json ValidItemsJson() {
    return nlohmann::json{
        {"capacity", 36},
        {"items",
         nlohmann::json::array(
             {{{"id", "parsnip"}, {"name", "Parsnip"}, {"category", "crop"},
               {"maxStack", 99}, {"sellPrice", 35}},
              {{"id", "cauliflower"}, {"name", "Cauliflower"}, {"category", "crop"},
               {"maxStack", 99}, {"sellPrice", 175}}})}};
}
} // namespace

TEST_CASE("ItemConfig:合法库存配置全字段解析") {
    auto cfg = LoadInventoryConfig(ValidItemsJson());
    REQUIRE(cfg.has_value());
    CHECK(cfg->capacity == 36);
    CHECK(cfg->items.Size() == 2);

    const auto* p = cfg->items.Find("parsnip");
    REQUIRE(p != nullptr);
    CHECK(p->name == "Parsnip");
    CHECK(p->category == "crop");
    CHECK(p->maxStack == 99);
    CHECK(p->sellPrice == 35);
    CHECK(cfg->items.Find("cauliflower") != nullptr);
    CHECK(cfg->items.Find("nonexistent") == nullptr);
}

TEST_CASE("ItemConfig:非法配置返回 nullopt") {
    SUBCASE("顶层非对象") {
        CHECK_FALSE(LoadInventoryConfig(nlohmann::json::array()).has_value());
    }
    SUBCASE("缺 capacity") {
        auto j = ValidItemsJson();
        j.erase("capacity");
        CHECK_FALSE(LoadInventoryConfig(j).has_value());
    }
    SUBCASE("capacity < 1") {
        auto j = ValidItemsJson();
        j["capacity"] = 0;
        CHECK_FALSE(LoadInventoryConfig(j).has_value());
    }
    SUBCASE("items 非数组") {
        auto j = ValidItemsJson();
        j["items"] = nlohmann::json::object();
        CHECK_FALSE(LoadInventoryConfig(j).has_value());
    }
    SUBCASE("缺字段 name") {
        auto j = ValidItemsJson();
        j["items"][0].erase("name");
        CHECK_FALSE(LoadInventoryConfig(j).has_value());
    }
    SUBCASE("maxStack < 1") {
        auto j = ValidItemsJson();
        j["items"][0]["maxStack"] = 0;
        CHECK_FALSE(LoadInventoryConfig(j).has_value());
    }
    SUBCASE("sellPrice < 0") {
        auto j = ValidItemsJson();
        j["items"][0]["sellPrice"] = -1;
        CHECK_FALSE(LoadInventoryConfig(j).has_value());
    }
    SUBCASE("空 id") {
        auto j = ValidItemsJson();
        j["items"][0]["id"] = "";
        CHECK_FALSE(LoadInventoryConfig(j).has_value());
    }
    SUBCASE("id 重复") {
        auto j = ValidItemsJson();
        j["items"][1]["id"] = "parsnip";
        CHECK_FALSE(LoadInventoryConfig(j).has_value());
    }
    SUBCASE("maxStack 类型错(字符串)") {
        auto j = ValidItemsJson();
        j["items"][0]["maxStack"] = "99";
        CHECK_FALSE(LoadInventoryConfig(j).has_value());
    }
}
