#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "me/core/Transform2D.h"
#include "me/scene/Scene.h" // me::scene::EntityId

namespace me::editor {

/// @brief Hierarchy 面板一行:来自 scene.list_entities 的单个实体。
struct EntityRow {
    me::scene::EntityId id = 0;
    me::Transform2D localTransform;
};

/// @brief Inspector 详情:来自 scene.get_entity(本切片无组件列表,见 spec §5)。
struct EntityDetails {
    me::scene::EntityId id = 0;
    me::Transform2D localTransform;
    me::scene::EntityId parentId = 0;
    std::vector<me::scene::EntityId> children;
};

/// @brief Log 面板一行:来自 log.read 的单条审计记录(取四字段)。
struct LogRow {
    std::uint64_t invocationId = 0;
    std::string toolName;
    bool ok = true;
    std::string code;
};

} // namespace me::editor
