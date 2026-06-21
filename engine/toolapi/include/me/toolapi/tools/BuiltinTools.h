#pragma once

#include <memory>

#include <nlohmann/json.hpp>

#include "me/core/Transform2D.h"
#include "me/toolapi/ITool.h"

namespace me::toolapi {

class ToolRegistry;

/// @brief Transform2D → JSON { position:{x,y}, rotation, scale:{x,y} }(供各 Tool 复用)。
nlohmann::json TransformToJson(const me::Transform2D& t);

// —— 查询型 Tool 工厂 ——
std::unique_ptr<ITool> MakeListEntitiesTool(); ///< scene.list_entities
std::unique_ptr<ITool> MakeGetEntityTool();    ///< scene.get_entity
std::unique_ptr<ITool> MakeLogReadTool();      ///< log.read

// —— 变更型 Tool 工厂 ——
std::unique_ptr<ITool> MakeCreateEntityTool();  ///< scene.create_entity
std::unique_ptr<ITool> MakeDestroyEntityTool(); ///< scene.destroy_entity
std::unique_ptr<ITool> MakeSetTransformTool();  ///< entity.set_transform

/// @brief 把 M6 首批全部 Tool 注册进 registry。
void RegisterBuiltinTools(ToolRegistry& registry);

} // namespace me::toolapi
