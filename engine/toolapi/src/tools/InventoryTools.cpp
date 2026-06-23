#include <memory>

#include "me/domain/Inventory.h"
#include "me/toolapi/ToolContext.h"
#include "me/toolapi/tools/BuiltinTools.h"

namespace me::toolapi {

nlohmann::json InventoryToJson(const me::domain::Inventory& inv) {
    nlohmann::json slots = nlohmann::json::array();
    int used = 0;
    const auto& s = inv.Slots();
    for (int i = 0; i < static_cast<int>(s.size()); ++i) {
        slots.push_back({{"slot", i}, {"itemId", s[i].itemId}, {"count", s[i].count}});
        if (!s[i].Empty()) ++used;
    }
    return nlohmann::json{{"capacity", inv.Capacity()}, {"used", used}, {"slots", slots}};
}

namespace {

/// @brief inventory.get:返回全部格位快照(只读查询)。
class InventoryGetTool final : public ITool {
public:
    std::string name() const override { return "inventory.get"; }
    ToolCategory category() const override { return ToolCategory::Query; }
    Permission permission() const override { return Permission::AgentAllowed; }
    nlohmann::json paramsSchema() const override {
        return {{"type", "object"}, {"properties", nlohmann::json::object()}};
    }
    ToolResult dryRun(ToolContext& ctx, const nlohmann::json& p) const override {
        return run(ctx, p); // 只读:预览即结果
    }
    ToolResult run(ToolContext& ctx, const nlohmann::json&) const override {
        if (ctx.inventory == nullptr)
            return ToolResult::Error(ToolErrorCode::PreconditionFailed,
                                     "no inventory wired into ToolContext");
        return ToolResult::Success(InventoryToJson(*ctx.inventory));
    }
};

/// @brief inventory.add:加入物品。运行时态,不经 CommandStack。
class InventoryAddTool final : public ITool {
public:
    std::string name() const override { return "inventory.add"; }
    ToolCategory category() const override { return ToolCategory::Mutation; }
    Permission permission() const override { return Permission::Automation; }
    nlohmann::json paramsSchema() const override {
        return {{"type", "object"},
                {"required", {"itemId", "count"}},
                {"properties",
                 {{"itemId", {{"type", "string"}}},
                  {"count", {{"type", "integer"}, {"minimum", 1}}}}}};
    }
    ToolResult dryRun(ToolContext& ctx, const nlohmann::json& p) const override {
        if (ctx.inventory == nullptr)
            return ToolResult::Error(ToolErrorCode::PreconditionFailed,
                                     "no inventory wired into ToolContext");
        me::domain::Inventory preview = *ctx.inventory; // 值拷贝:零副作用
        return apply(preview, p);
    }
    ToolResult run(ToolContext& ctx, const nlohmann::json& p) const override {
        if (ctx.inventory == nullptr)
            return ToolResult::Error(ToolErrorCode::PreconditionFailed,
                                     "no inventory wired into ToolContext");
        return apply(*ctx.inventory, p);
    }
private:
    static ToolResult apply(me::domain::Inventory& inv, const nlohmann::json& p) {
        const std::string itemId = p["itemId"].get<std::string>();
        const int count = p["count"].get<int>();
        switch (inv.Add(itemId, count).status) {
            case me::domain::AddStatus::UnknownItem:
                return ToolResult::Error(ToolErrorCode::PreconditionFailed, "unknown itemId");
            case me::domain::AddStatus::Full:
                return ToolResult::Error(ToolErrorCode::PreconditionFailed, "inventory full");
            case me::domain::AddStatus::Ok:
                break;
        }
        return ToolResult::Success(InventoryToJson(inv));
    }
};

/// @brief inventory.remove:移除物品(EditorOnly:销毁性,运行时不可 Undo)。
class InventoryRemoveTool final : public ITool {
public:
    std::string name() const override { return "inventory.remove"; }
    ToolCategory category() const override { return ToolCategory::Mutation; }
    Permission permission() const override { return Permission::EditorOnly; }
    nlohmann::json paramsSchema() const override {
        return {{"type", "object"},
                {"required", {"itemId", "count"}},
                {"properties",
                 {{"itemId", {{"type", "string"}}},
                  {"count", {{"type", "integer"}, {"minimum", 1}}}}}};
    }
    ToolResult dryRun(ToolContext& ctx, const nlohmann::json& p) const override {
        if (ctx.inventory == nullptr)
            return ToolResult::Error(ToolErrorCode::PreconditionFailed,
                                     "no inventory wired into ToolContext");
        me::domain::Inventory preview = *ctx.inventory; // 值拷贝:零副作用
        return apply(preview, p);
    }
    ToolResult run(ToolContext& ctx, const nlohmann::json& p) const override {
        if (ctx.inventory == nullptr)
            return ToolResult::Error(ToolErrorCode::PreconditionFailed,
                                     "no inventory wired into ToolContext");
        return apply(*ctx.inventory, p);
    }
private:
    static ToolResult apply(me::domain::Inventory& inv, const nlohmann::json& p) {
        const std::string itemId = p["itemId"].get<std::string>();
        const int count = p["count"].get<int>();
        if (inv.Remove(itemId, count).status == me::domain::RemoveStatus::NotEnough)
            return ToolResult::Error(ToolErrorCode::PreconditionFailed,
                                     "not enough items to remove");
        return ToolResult::Success(InventoryToJson(inv));
    }
};

} // namespace

std::unique_ptr<ITool> MakeInventoryGetTool() { return std::make_unique<InventoryGetTool>(); }
std::unique_ptr<ITool> MakeInventoryAddTool() { return std::make_unique<InventoryAddTool>(); }
std::unique_ptr<ITool> MakeInventoryRemoveTool() { return std::make_unique<InventoryRemoveTool>(); }

} // namespace me::toolapi
