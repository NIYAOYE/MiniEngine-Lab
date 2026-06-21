#pragma once

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "me/core/Transform2D.h"
#include "me/editor/EditorDto.h"
#include "me/scene/Scene.h"
#include "me/toolapi/ToolContext.h"
#include "me/toolapi/ToolRegistry.h"
#include "me/toolapi/ToolResult.h"

namespace me::editor {

/**
 * @brief 编辑器决策核心:把 UI 意图翻译为 Tool 调用,只经 ToolRegistry 读写引擎。
 *
 * CPU-only、不依赖 ImGui/RHI,故可在 WSL 单测。读走查询 Tool、写走变更 Tool;
 * Undo/Redo 因 M6 未暴露为 Tool,作为特权宿主直调 CommandStack(见 spec §5)。
 */
class EditorController {
public:
    /// @brief 无效实体哨兵(无选中)。
    static constexpr me::scene::EntityId kInvalidEntityId = 0;

    /// @brief 注入受控门面;controller 不持有所有权。
    EditorController(me::toolapi::ToolRegistry& registry, me::toolapi::ToolContext& ctx);

    // —— 查询(经查询 Tool)——
    /// @brief 经 scene.list_entities 刷新 Hierarchy 缓存。
    void RefreshHierarchy();
    const std::vector<EntityRow>& Hierarchy() const { return m_hierarchy; }

    /// @brief 选中实体(本地状态)。
    void Select(me::scene::EntityId id);
    me::scene::EntityId Selected() const { return m_selected; }
    bool HasSelection() const { return m_selected != kInvalidEntityId; }
    /// @brief 经 scene.get_entity 刷新选中实体详情。
    void InspectSelected();
    const EntityDetails& Inspected() const { return m_inspected; }
    bool HasInspected() const { return m_hasInspected; }

    /// @brief 经 log.read 刷新审计日志缓存。
    void RefreshLog();
    const std::vector<LogRow>& Log() const { return m_log; }

    // —— 变更(经变更 Tool)——
    /// @brief 经 scene.create_entity 创建实体,成功后选中并刷新。
    void CreateEntity();
    /// @brief 经 scene.destroy_entity 销毁选中实体,成功后清空选中并刷新。
    void DestroySelected();
    /// @brief 经 entity.set_transform 覆盖选中实体的局部变换。
    void ApplyTransform(const me::Transform2D& t);

    // —— 命令栈元操作(M6 无对应 Tool,特权宿主直调,见 spec §5)——
    void Undo();
    void Redo();
    bool CanUndo() const;
    bool CanRedo() const;

    // —— 错误暴露(绝不静默)——
    const std::string& LastError() const { return m_lastError; }
    bool HasError() const { return !m_lastError.empty(); }
    void ClearError() { m_lastError.clear(); }

private:
    /// @brief 统一 Invoke:失败写 m_lastError、成功清空;以 Editor 角色调用。
    me::toolapi::ToolResult invoke(const std::string& name, const nlohmann::json& params);
    /// @brief 刷新后核对选中是否仍存活;失效则清空选中与详情。
    void reconcileSelection();

    me::toolapi::ToolRegistry& m_registry;
    me::toolapi::ToolContext& m_ctx;

    std::vector<EntityRow> m_hierarchy;
    me::scene::EntityId m_selected = kInvalidEntityId;
    EntityDetails m_inspected;
    bool m_hasInspected = false;
    std::vector<LogRow> m_log;
    std::string m_lastError;
};

} // namespace me::editor
