#include <memory>

#include "me/domain/FarmField.h"
#include "me/domain/Inventory.h"
#include "me/toolapi/ToolContext.h"
#include "me/toolapi/tools/BuiltinTools.h"

namespace me::toolapi {

nlohmann::json CropInstanceToJson(int x, int y, const me::domain::CropInstance& c,
                                  const me::domain::CropDatabase& db) {
    const me::domain::CropConfig* cfg = db.Find(c.cropId);
    std::string stageName = cfg ? cfg->stageNames[c.stage] : std::string{};
    bool mature = cfg ? cfg->IsMatureStage(c.stage) : false;
    return nlohmann::json{
        {"x", x},           {"y", y},
        {"cropId", c.cropId},{"stage", c.stage},
        {"stageName", stageName},{"daysInStage", c.daysInStage},
        {"watered", c.watered},{"mature", mature},
    };
}

namespace {

// 把整片农田序列化为 { crops:[...] }(键有序,确定性)。
nlohmann::json FieldToJson(const me::domain::FarmField& farm) {
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& [key, crop] : farm.Crops())
        arr.push_back(CropInstanceToJson(key.x, key.y, crop, farm.Database()));
    return nlohmann::json{{"crops", arr}};
}

// crop.get_field:返回全部作物(只读)。
class CropGetFieldTool final : public ITool {
public:
    std::string name() const override { return "crop.get_field"; }
    ToolCategory category() const override { return ToolCategory::Query; }
    Permission permission() const override { return Permission::AgentAllowed; }
    nlohmann::json paramsSchema() const override {
        return {{"type", "object"}, {"properties", nlohmann::json::object()}};
    }
    ToolResult dryRun(ToolContext& ctx, const nlohmann::json& p) const override {
        return run(ctx, p); // 只读:预览即结果
    }
    ToolResult run(ToolContext& ctx, const nlohmann::json&) const override {
        if (ctx.farm == nullptr)
            return ToolResult::Error(ToolErrorCode::PreconditionFailed,
                                     "no farm field wired into ToolContext");
        return ToolResult::Success(FieldToJson(*ctx.farm));
    }
};

// crop.plant:在瓦片种植作物。运行时态,不经 CommandStack。
class CropPlantTool final : public ITool {
public:
    std::string name() const override { return "crop.plant"; }
    ToolCategory category() const override { return ToolCategory::Mutation; }
    Permission permission() const override { return Permission::Automation; }
    nlohmann::json paramsSchema() const override {
        return {{"type", "object"},
                {"required", {"tileX", "tileY", "cropId"}},
                {"properties",
                 {{"tileX", {{"type", "integer"}}},
                  {"tileY", {{"type", "integer"}}},
                  {"cropId", {{"type", "string"}}}}}};
    }
    ToolResult dryRun(ToolContext& ctx, const nlohmann::json& p) const override {
        if (ctx.farm == nullptr)
            return ToolResult::Error(ToolErrorCode::PreconditionFailed,
                                     "no farm field wired into ToolContext");
        me::domain::FarmField preview = *ctx.farm; // 值拷贝:零副作用
        return apply(preview, p);
    }
    ToolResult run(ToolContext& ctx, const nlohmann::json& p) const override {
        if (ctx.farm == nullptr)
            return ToolResult::Error(ToolErrorCode::PreconditionFailed,
                                     "no farm field wired into ToolContext");
        return apply(*ctx.farm, p);
    }
private:
    static ToolResult apply(me::domain::FarmField& farm, const nlohmann::json& p) {
        const int x = p["tileX"].get<int>();
        const int y = p["tileY"].get<int>();
        const std::string cropId = p["cropId"].get<std::string>();
        switch (farm.Plant(x, y, cropId)) {
            case me::domain::PlantStatus::TileOccupied:
                return ToolResult::Error(ToolErrorCode::PreconditionFailed,
                                         "tile already has a crop");
            case me::domain::PlantStatus::UnknownCrop:
                return ToolResult::Error(ToolErrorCode::PreconditionFailed,
                                         "unknown cropId");
            case me::domain::PlantStatus::Ok:
                break;
        }
        return ToolResult::Success(
            CropInstanceToJson(x, y, *farm.At(x, y), farm.Database()));
    }
};

// crop.water:给瓦片浇水。运行时态,不经 CommandStack。
class CropWaterTool final : public ITool {
public:
    std::string name() const override { return "crop.water"; }
    ToolCategory category() const override { return ToolCategory::Mutation; }
    Permission permission() const override { return Permission::Automation; }
    nlohmann::json paramsSchema() const override {
        return {{"type", "object"},
                {"required", {"tileX", "tileY"}},
                {"properties",
                 {{"tileX", {{"type", "integer"}}},
                  {"tileY", {{"type", "integer"}}}}}};
    }
    ToolResult dryRun(ToolContext& ctx, const nlohmann::json& p) const override {
        if (ctx.farm == nullptr)
            return ToolResult::Error(ToolErrorCode::PreconditionFailed,
                                     "no farm field wired into ToolContext");
        me::domain::FarmField preview = *ctx.farm;
        return apply(preview, p);
    }
    ToolResult run(ToolContext& ctx, const nlohmann::json& p) const override {
        if (ctx.farm == nullptr)
            return ToolResult::Error(ToolErrorCode::PreconditionFailed,
                                     "no farm field wired into ToolContext");
        return apply(*ctx.farm, p);
    }
private:
    static ToolResult apply(me::domain::FarmField& farm, const nlohmann::json& p) {
        const int x = p["tileX"].get<int>();
        const int y = p["tileY"].get<int>();
        if (!farm.Water(x, y))
            return ToolResult::Error(ToolErrorCode::PreconditionFailed,
                                     "no crop on tile to water");
        return ToolResult::Success(
            CropInstanceToJson(x, y, *farm.At(x, y), farm.Database()));
    }
};

// crop.advance_days:推进 N 天(运行时态,不经 CommandStack)。
class CropAdvanceDaysTool final : public ITool {
public:
    std::string name() const override { return "crop.advance_days"; }
    ToolCategory category() const override { return ToolCategory::Mutation; }
    Permission permission() const override { return Permission::Automation; }
    nlohmann::json paramsSchema() const override {
        return {{"type", "object"},
                {"required", {"days"}},
                {"properties", {{"days", {{"type", "integer"}, {"minimum", 1}}}}}};
    }
    ToolResult dryRun(ToolContext& ctx, const nlohmann::json& p) const override {
        if (ctx.farm == nullptr)
            return ToolResult::Error(ToolErrorCode::PreconditionFailed,
                                     "no farm field wired into ToolContext");
        me::domain::FarmField preview = *ctx.farm; // 值拷贝:零副作用
        return apply(preview, p);
    }
    ToolResult run(ToolContext& ctx, const nlohmann::json& p) const override {
        if (ctx.farm == nullptr)
            return ToolResult::Error(ToolErrorCode::PreconditionFailed,
                                     "no farm field wired into ToolContext");
        return apply(*ctx.farm, p);
    }
private:
    static ToolResult apply(me::domain::FarmField& farm, const nlohmann::json& p) {
        const int days = p["days"].get<int>();
        farm.AdvanceDays(days);
        nlohmann::json out = FieldToJson(farm);
        out["advanced"] = days;
        return ToolResult::Success(out);
    }
};

// crop.harvest:收获成熟作物(EditorOnly:销毁性产出,清空瓦片)。运行时态,不经 CommandStack。
class CropHarvestTool final : public ITool {
public:
    std::string name() const override { return "crop.harvest"; }
    ToolCategory category() const override { return ToolCategory::Mutation; }
    Permission permission() const override { return Permission::EditorOnly; }
    nlohmann::json paramsSchema() const override {
        return {{"type", "object"},
                {"required", {"tileX", "tileY"}},
                {"properties",
                 {{"tileX", {{"type", "integer"}}},
                  {"tileY", {{"type", "integer"}}}}}};
    }
    ToolResult dryRun(ToolContext& ctx, const nlohmann::json& p) const override {
        if (ctx.farm == nullptr)
            return ToolResult::Error(ToolErrorCode::PreconditionFailed,
                                     "no farm field wired into ToolContext");
        me::domain::FarmField farmCopy = *ctx.farm; // 值拷贝:零副作用
        if (ctx.inventory != nullptr) {
            me::domain::Inventory invCopy = *ctx.inventory;
            return apply(farmCopy, &invCopy, p);
        }
        return apply(farmCopy, nullptr, p);
    }
    ToolResult run(ToolContext& ctx, const nlohmann::json& p) const override {
        if (ctx.farm == nullptr)
            return ToolResult::Error(ToolErrorCode::PreconditionFailed,
                                     "no farm field wired into ToolContext");
        return apply(*ctx.farm, ctx.inventory, p);
    }
private:
    // 原子收获:先预判库存可容纳,再清瓦片入库;库满则瓦片不清。
    static ToolResult apply(me::domain::FarmField& farm, me::domain::Inventory* inv,
                            const nlohmann::json& p) {
        const int x = p["tileX"].get<int>();
        const int y = p["tileY"].get<int>();
        const me::domain::HarvestResult peek = farm.PeekHarvest(x, y);
        switch (peek.status) {
            case me::domain::HarvestStatus::EmptyTile:
                return ToolResult::Error(ToolErrorCode::PreconditionFailed,
                                         "no crop on tile to harvest");
            case me::domain::HarvestStatus::NotMature:
                return ToolResult::Error(ToolErrorCode::PreconditionFailed,
                                         "crop not mature");
            case me::domain::HarvestStatus::Ok:
                break;
        }
        if (inv != nullptr && !inv->CanAdd(peek.itemId, peek.count))
            return ToolResult::Error(ToolErrorCode::PreconditionFailed, "inventory full");

        farm.Harvest(x, y); // 状态 Ok,清瓦片
        bool added = false;
        if (inv != nullptr) {
            inv->Add(peek.itemId, peek.count); // CanAdd 已保证成功
            added = true;
        }
        return ToolResult::Success(
            {{"itemId", peek.itemId}, {"count", peek.count}, {"addedToInventory", added}});
    }
};

} // namespace

std::unique_ptr<ITool> MakeCropGetFieldTool() { return std::make_unique<CropGetFieldTool>(); }
std::unique_ptr<ITool> MakeCropPlantTool() { return std::make_unique<CropPlantTool>(); }
std::unique_ptr<ITool> MakeCropWaterTool() { return std::make_unique<CropWaterTool>(); }
std::unique_ptr<ITool> MakeCropAdvanceDaysTool() { return std::make_unique<CropAdvanceDaysTool>(); }
std::unique_ptr<ITool> MakeCropHarvestTool() { return std::make_unique<CropHarvestTool>(); }

} // namespace me::toolapi
