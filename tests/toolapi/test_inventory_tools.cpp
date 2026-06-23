#include <doctest/doctest.h>

#include "me/command/CommandStack.h"
#include "me/domain/Inventory.h"
#include "me/scene/Scene.h"
#include "me/toolapi/ToolContext.h"
#include "me/toolapi/ToolRegistry.h"
#include "me/toolapi/tools/BuiltinTools.h"

using namespace me::toolapi;

namespace {
me::domain::Inventory MakeInv() {
    auto cfg = me::domain::LoadInventoryConfig(nlohmann::json{
        {"capacity", 4},
        {"items",
         nlohmann::json::array(
             {{{"id", "parsnip"}, {"name", "Parsnip"}, {"category", "crop"},
               {"maxStack", 5}, {"sellPrice", 35}}})}});
    REQUIRE(cfg.has_value());
    return me::domain::Inventory(cfg->items, cfg->capacity);
}
} // namespace

TEST_CASE("InventoryTools:add 入库 get 可见") {
    ToolRegistry reg;
    reg.Register(MakeInventoryAddTool());
    reg.Register(MakeInventoryGetTool());
    me::scene::Scene scene;
    me::command::CommandStack stack;
    ToolInvocationLog log;
    auto inv = MakeInv();
    ToolContext ctx{scene, stack, log, nullptr, nullptr, &inv};

    auto a = reg.Invoke("inventory.add", {{"itemId", "parsnip"}, {"count", 3}},
                        CallerRole::Automation, ctx);
    REQUIRE(a.ok);

    auto g = reg.Invoke("inventory.get", nlohmann::json::object(), CallerRole::Agent, ctx);
    REQUIRE(g.ok);
    CHECK(g.data["capacity"] == 4);
    CHECK(g.data["used"] == 1);
    CHECK(g.data["slots"][0]["itemId"] == "parsnip");
    CHECK(g.data["slots"][0]["count"] == 3);
}

TEST_CASE("InventoryTools:add 未知物品 PreconditionFailed") {
    ToolRegistry reg;
    reg.Register(MakeInventoryAddTool());
    me::scene::Scene scene;
    me::command::CommandStack stack;
    ToolInvocationLog log;
    auto inv = MakeInv();
    ToolContext ctx{scene, stack, log, nullptr, nullptr, &inv};

    auto r = reg.Invoke("inventory.add", {{"itemId", "banana"}, {"count", 1}},
                        CallerRole::Automation, ctx);
    CHECK_FALSE(r.ok);
    CHECK(r.code == ToolErrorCode::PreconditionFailed);
}

TEST_CASE("InventoryTools:add 库满 PreconditionFailed") {
    ToolRegistry reg;
    reg.Register(MakeInventoryAddTool());
    me::scene::Scene scene;
    me::command::CommandStack stack;
    ToolInvocationLog log;
    auto cfg = me::domain::LoadInventoryConfig(nlohmann::json{
        {"capacity", 1},
        {"items", nlohmann::json::array({{{"id", "parsnip"}, {"name", "Parsnip"},
                                          {"category", "crop"}, {"maxStack", 5},
                                          {"sellPrice", 35}}})}});
    REQUIRE(cfg.has_value());
    me::domain::Inventory inv(cfg->items, cfg->capacity);
    ToolContext ctx{scene, stack, log, nullptr, nullptr, &inv};

    REQUIRE(reg.Invoke("inventory.add", {{"itemId", "parsnip"}, {"count", 5}},
                       CallerRole::Automation, ctx).ok);
    auto r = reg.Invoke("inventory.add", {{"itemId", "parsnip"}, {"count", 1}},
                        CallerRole::Automation, ctx);
    CHECK_FALSE(r.ok);
    CHECK(r.code == ToolErrorCode::PreconditionFailed);
}

TEST_CASE("InventoryTools:remove 不足 PreconditionFailed") {
    ToolRegistry reg;
    reg.Register(MakeInventoryRemoveTool());
    me::scene::Scene scene;
    me::command::CommandStack stack;
    ToolInvocationLog log;
    auto inv = MakeInv();
    ToolContext ctx{scene, stack, log, nullptr, nullptr, &inv};

    auto r = reg.Invoke("inventory.remove", {{"itemId", "parsnip"}, {"count", 1}},
                        CallerRole::Editor, ctx);
    CHECK_FALSE(r.ok);
    CHECK(r.code == ToolErrorCode::PreconditionFailed);
}

TEST_CASE("InventoryTools:add dry-run 零副作用") {
    ToolRegistry reg;
    reg.Register(MakeInventoryAddTool());
    me::scene::Scene scene;
    me::command::CommandStack stack;
    ToolInvocationLog log;
    auto inv = MakeInv();
    ToolContext ctx{scene, stack, log, nullptr, nullptr, &inv};

    auto r = reg.Invoke("inventory.add", {{"itemId", "parsnip"}, {"count", 2}},
                        CallerRole::Automation, ctx, /*dryRun=*/true);
    REQUIRE(r.ok);
    CHECK(inv.CountOf("parsnip") == 0); // 真身未变
}

TEST_CASE("InventoryTools:权限——remove 拒绝 Agent/Automation,get 放行 Agent") {
    ToolRegistry reg;
    reg.Register(MakeInventoryGetTool());
    reg.Register(MakeInventoryRemoveTool());
    me::scene::Scene scene;
    me::command::CommandStack stack;
    ToolInvocationLog log;
    auto inv = MakeInv();
    ToolContext ctx{scene, stack, log, nullptr, nullptr, &inv};

    CHECK(reg.Invoke("inventory.get", nlohmann::json::object(), CallerRole::Agent, ctx).ok);
    auto a = reg.Invoke("inventory.remove", {{"itemId", "parsnip"}, {"count", 1}},
                        CallerRole::Agent, ctx);
    CHECK(a.code == ToolErrorCode::PermissionDenied);
    auto m = reg.Invoke("inventory.remove", {{"itemId", "parsnip"}, {"count", 1}},
                        CallerRole::Automation, ctx);
    CHECK(m.code == ToolErrorCode::PermissionDenied);
}

TEST_CASE("InventoryTools:add schema 拒绝 count<1") {
    ToolRegistry reg;
    reg.Register(MakeInventoryAddTool());
    me::scene::Scene scene;
    me::command::CommandStack stack;
    ToolInvocationLog log;
    auto inv = MakeInv();
    ToolContext ctx{scene, stack, log, nullptr, nullptr, &inv};

    auto r = reg.Invoke("inventory.add", {{"itemId", "parsnip"}, {"count", 0}},
                        CallerRole::Automation, ctx);
    CHECK_FALSE(r.ok);
    CHECK(r.code == ToolErrorCode::InvalidParams);
}

TEST_CASE("InventoryTools:get 无库存 PreconditionFailed") {
    ToolRegistry reg;
    reg.Register(MakeInventoryGetTool());
    me::scene::Scene scene;
    me::command::CommandStack stack;
    ToolInvocationLog log;
    ToolContext ctx{scene, stack, log}; // inventory = nullptr

    auto r = reg.Invoke("inventory.get", nlohmann::json::object(), CallerRole::Agent, ctx);
    CHECK_FALSE(r.ok);
    CHECK(r.code == ToolErrorCode::PreconditionFailed);
}
