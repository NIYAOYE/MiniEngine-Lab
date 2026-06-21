#include <memory>

#include "me/command/commands/CreateEntityCmd.h"
#include "me/command/commands/DestroyEntityCmd.h"
#include "me/command/commands/SetTransformCmd.h"
#include "me/scene/Scene.h"
#include "me/toolapi/ToolContext.h"
#include "me/toolapi/tools/BuiltinTools.h"

namespace me::toolapi {
namespace {

// scene.create_entity:创建单位变换、无父的新实体。
class CreateEntityTool final : public ITool {
public:
    std::string name() const override { return "scene.create_entity"; }
    ToolCategory category() const override { return ToolCategory::Mutation; }
    Permission permission() const override { return Permission::Automation; }
    nlohmann::json paramsSchema() const override {
        return {{"type", "object"}, {"properties", nlohmann::json::object()}};
    }
    ToolResult dryRun(ToolContext&, const nlohmann::json&) const override {
        return ToolResult::Success({{"preview", "create entity"}});
    }
    ToolResult run(ToolContext& ctx, const nlohmann::json&) const override {
        auto cmd = std::make_unique<me::command::CreateEntityCmd>();
        me::command::CreateEntityCmd* raw = cmd.get();
        const auto res = ctx.commands.execute(std::move(cmd), ctx.scene);
        if (!res.ok)
            return ToolResult::Error(ToolErrorCode::ExecutionFailed, res.message);
        return ToolResult::Success({{"id", raw->CreatedId()}}, "entity created");
    }
};

// scene.destroy_entity:销毁实体及其子树(经 Command,可 undo 还原)。
class DestroyEntityTool final : public ITool {
public:
    std::string name() const override { return "scene.destroy_entity"; }
    ToolCategory category() const override { return ToolCategory::Mutation; }
    Permission permission() const override { return Permission::EditorOnly; }
    nlohmann::json paramsSchema() const override {
        return {{"type", "object"},
                {"required", {"id"}},
                {"properties", {{"id", {{"type", "integer"}, {"minimum", 1}}}}}};
    }
    ToolResult dryRun(ToolContext& ctx, const nlohmann::json& p) const override {
        const auto id = p["id"].get<me::scene::EntityId>();
        if (!ctx.scene.IsAlive(ctx.scene.Resolve(id)))
            return ToolResult::Error(ToolErrorCode::PreconditionFailed,
                                     "no such entity: id=" + std::to_string(id));
        return ToolResult::Success({{"preview", "destroy entity id=" + std::to_string(id)}});
    }
    ToolResult run(ToolContext& ctx, const nlohmann::json& p) const override {
        const auto id = p["id"].get<me::scene::EntityId>();
        if (!ctx.scene.IsAlive(ctx.scene.Resolve(id)))
            return ToolResult::Error(ToolErrorCode::PreconditionFailed,
                                     "no such entity: id=" + std::to_string(id));
        auto cmd = std::make_unique<me::command::DestroyEntityCmd>(id);
        const auto res = ctx.commands.execute(std::move(cmd), ctx.scene);
        if (!res.ok)
            return ToolResult::Error(ToolErrorCode::ExecutionFailed, res.message);
        return ToolResult::Success({{"id", id}}, "entity destroyed");
    }
};

// entity.set_transform:以现有局部变换为基,覆盖给定字段。
class SetTransformTool final : public ITool {
public:
    std::string name() const override { return "entity.set_transform"; }
    ToolCategory category() const override { return ToolCategory::Mutation; }
    Permission permission() const override { return Permission::Automation; }
    nlohmann::json paramsSchema() const override {
        return {
            {"type", "object"},
            {"required", {"id"}},
            {"properties",
             {{"id", {{"type", "integer"}, {"minimum", 1}}},
              {"rotation", {{"type", "number"}}},
              {"position", {{"type", "object"},
                            {"properties", {{"x", {{"type", "number"}}},
                                            {"y", {{"type", "number"}}}}}}},
              {"scale", {{"type", "object"},
                         {"properties", {{"x", {{"type", "number"}}},
                                         {"y", {{"type", "number"}}}}}}}}}};
    }
    ToolResult dryRun(ToolContext& ctx, const nlohmann::json& p) const override {
        const auto id = p["id"].get<me::scene::EntityId>();
        if (!ctx.scene.IsAlive(ctx.scene.Resolve(id)))
            return ToolResult::Error(ToolErrorCode::PreconditionFailed,
                                     "no such entity: id=" + std::to_string(id));
        return ToolResult::Success(
            {{"preview", "set transform on id=" + std::to_string(id)}});
    }
    ToolResult run(ToolContext& ctx, const nlohmann::json& p) const override {
        const auto id = p["id"].get<me::scene::EntityId>();
        const me::scene::Entity e = ctx.scene.Resolve(id);
        if (!ctx.scene.IsAlive(e))
            return ToolResult::Error(ToolErrorCode::PreconditionFailed,
                                     "no such entity: id=" + std::to_string(id));
        // 以当前局部变换为基,逐字段覆盖(未给字段保持原值)。
        me::Transform2D t = ctx.scene.LocalTransform(e);
        if (p.contains("position")) {
            const auto& pos = p["position"];
            if (pos.contains("x")) t.position.x = pos["x"].get<float>();
            if (pos.contains("y")) t.position.y = pos["y"].get<float>();
        }
        if (p.contains("rotation")) t.rotation = p["rotation"].get<float>();
        if (p.contains("scale")) {
            const auto& s = p["scale"];
            if (s.contains("x")) t.scale.x = s["x"].get<float>();
            if (s.contains("y")) t.scale.y = s["y"].get<float>();
        }
        auto cmd = std::make_unique<me::command::SetTransformCmd>(id, t);
        const auto res = ctx.commands.execute(std::move(cmd), ctx.scene);
        if (!res.ok)
            return ToolResult::Error(ToolErrorCode::ExecutionFailed, res.message);
        return ToolResult::Success({{"id", id}}, "transform set");
    }
};

} // namespace

std::unique_ptr<ITool> MakeCreateEntityTool() { return std::make_unique<CreateEntityTool>(); }
std::unique_ptr<ITool> MakeDestroyEntityTool() { return std::make_unique<DestroyEntityTool>(); }
std::unique_ptr<ITool> MakeSetTransformTool() { return std::make_unique<SetTransformTool>(); }

} // namespace me::toolapi
