#include "me/scene/Scene.h"
#include "me/toolapi/ToolContext.h"
#include "me/toolapi/tools/BuiltinTools.h"

namespace me::toolapi {

nlohmann::json TransformToJson(const me::Transform2D& t) {
    return nlohmann::json{
        {"position", {{"x", t.position.x}, {"y", t.position.y}}},
        {"rotation", t.rotation},
        {"scale", {{"x", t.scale.x}, {"y", t.scale.y}}},
    };
}

namespace {

// scene.list_entities:列出全部存活实体的身份与局部变换。
class ListEntitiesTool final : public ITool {
public:
    std::string name() const override { return "scene.list_entities"; }
    ToolCategory category() const override { return ToolCategory::Query; }
    Permission permission() const override { return Permission::AgentAllowed; }
    nlohmann::json paramsSchema() const override {
        return {{"type", "object"}, {"properties", nlohmann::json::object()}};
    }
    ToolResult dryRun(ToolContext& ctx, const nlohmann::json& p) const override {
        return run(ctx, p); // 只读:预览即结果
    }
    ToolResult run(ToolContext& ctx, const nlohmann::json&) const override {
        nlohmann::json entities = nlohmann::json::array();
        for (me::scene::Entity e : ctx.scene.AliveEntities()) {
            nlohmann::json item = TransformToJson(ctx.scene.LocalTransform(e));
            item["id"] = ctx.scene.IdOf(e);
            entities.push_back(std::move(item));
        }
        return ToolResult::Success(
            {{"count", entities.size()}, {"entities", std::move(entities)}});
    }
};

// scene.get_entity:按持久 id 取单个实体的层级与变换。
class GetEntityTool final : public ITool {
public:
    std::string name() const override { return "scene.get_entity"; }
    ToolCategory category() const override { return ToolCategory::Query; }
    Permission permission() const override { return Permission::AgentAllowed; }
    nlohmann::json paramsSchema() const override {
        return {{"type", "object"},
                {"required", {"id"}},
                {"properties", {{"id", {{"type", "integer"}, {"minimum", 1}}}}}};
    }
    ToolResult dryRun(ToolContext& ctx, const nlohmann::json& p) const override {
        return run(ctx, p);
    }
    ToolResult run(ToolContext& ctx, const nlohmann::json& p) const override {
        const auto id = p["id"].get<me::scene::EntityId>();
        const me::scene::Entity e = ctx.scene.Resolve(id);
        if (!ctx.scene.IsAlive(e))
            return ToolResult::Error(ToolErrorCode::PreconditionFailed,
                                     "no such entity: id=" + std::to_string(id));
        nlohmann::json data = TransformToJson(ctx.scene.LocalTransform(e));
        data["id"] = id;
        const me::scene::Entity parent = ctx.scene.Parent(e);
        data["parentId"] = ctx.scene.IsAlive(parent) ? ctx.scene.IdOf(parent) : 0;
        nlohmann::json children = nlohmann::json::array();
        for (me::scene::Entity c : ctx.scene.Children(e)) children.push_back(ctx.scene.IdOf(c));
        data["children"] = std::move(children);
        return ToolResult::Success(std::move(data));
    }
};

} // namespace

std::unique_ptr<ITool> MakeListEntitiesTool() { return std::make_unique<ListEntitiesTool>(); }
std::unique_ptr<ITool> MakeGetEntityTool() { return std::make_unique<GetEntityTool>(); }

} // namespace me::toolapi
