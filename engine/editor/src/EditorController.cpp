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

void EditorController::Select(me::scene::EntityId id) { m_selected = id; }

void EditorController::InspectSelected() {
    m_hasInspected = false;
    if (!HasSelection()) return;
    const me::toolapi::ToolResult r = invoke("scene.get_entity", {{"id", m_selected}});
    if (!r.ok) return;
    EntityDetails d;
    d.id = m_selected;
    if (!ReadTransform(r.data, d.localTransform)) {
        m_lastError = "malformed get_entity response";
        return;
    }
    d.parentId = r.data.value("parentId", me::scene::EntityId{0});
    if (r.data.contains("children"))
        for (const nlohmann::json& c : r.data["children"])
            d.children.push_back(c.get<me::scene::EntityId>());
    m_inspected = d;
    m_hasInspected = true;
}

void EditorController::CreateEntity() {
    const me::toolapi::ToolResult r = invoke("scene.create_entity", nlohmann::json::object());
    if (!r.ok) return;
    const auto id = r.data.value("id", me::scene::EntityId{0});
    RefreshHierarchy();
    if (id != kInvalidEntityId) {
        m_selected = id;
        InspectSelected();
    }
}

void EditorController::ApplyTransform(const me::Transform2D& t) {
    if (!HasSelection()) {
        m_lastError = "ApplyTransform: no selection";
        return;
    }
    const nlohmann::json params = {
        {"id", m_selected},
        {"position", {{"x", t.position.x}, {"y", t.position.y}}},
        {"rotation", t.rotation},
        {"scale", {{"x", t.scale.x}, {"y", t.scale.y}}},
    };
    const me::toolapi::ToolResult r = invoke("entity.set_transform", params);
    if (!r.ok) return;
    InspectSelected();
}

void EditorController::DestroySelected() {
    if (!HasSelection()) {
        m_lastError = "DestroySelected: no selection";
        return;
    }
    const me::toolapi::ToolResult r = invoke("scene.destroy_entity", {{"id", m_selected}});
    if (!r.ok) return;
    m_selected = kInvalidEntityId;
    m_hasInspected = false;
    RefreshHierarchy();
}

} // namespace me::editor
