#include "me/editor/EditorController.h"

namespace me::editor {
namespace {

// 从 {position:{x,y}, rotation, scale:{x,y}} 读 Transform2D;字段缺失返回 false。
// 不用 nlohmann .at()(会抛异常,违反项目规范),一律 contains 守卫。
bool ReadTransform(const nlohmann::json& j, me::Transform2D& out) {
    if (!j.contains("position") || !j.contains("rotation") || !j.contains("scale"))
        return false;
    const nlohmann::json& pos = j["position"];
    const nlohmann::json& sc = j["scale"];
    if (!pos.contains("x") || !pos.contains("y") || !sc.contains("x") || !sc.contains("y"))
        return false;
    out.position.x = pos["x"].get<float>();
    out.position.y = pos["y"].get<float>();
    out.rotation = j["rotation"].get<float>();
    out.scale.x = sc["x"].get<float>();
    out.scale.y = sc["y"].get<float>();
    return true;
}

} // namespace

EditorController::EditorController(me::toolapi::ToolRegistry& registry,
                                  me::toolapi::ToolContext& ctx)
    : m_registry(registry), m_ctx(ctx) {}

me::toolapi::ToolResult EditorController::invoke(const std::string& name,
                                                const nlohmann::json& params) {
    me::toolapi::ToolResult r =
        m_registry.Invoke(name, params, me::toolapi::CallerRole::Editor, m_ctx);
    if (!r.ok)
        m_lastError = std::string(me::toolapi::ToString(r.code)) + ": " + r.message;
    else
        m_lastError.clear();
    return r;
}

void EditorController::RefreshHierarchy() {
    m_hierarchy.clear();
    const me::toolapi::ToolResult r = invoke("scene.list_entities", nlohmann::json::object());
    if (!r.ok || !r.data.contains("entities")) return;
    for (const nlohmann::json& e : r.data["entities"]) {
        EntityRow row;
        if (!e.contains("id")) continue;
        row.id = e["id"].get<me::scene::EntityId>();
        ReadTransform(e, row.localTransform); // 失败保留默认单位变换
        m_hierarchy.push_back(row);
    }
}

} // namespace me::editor
