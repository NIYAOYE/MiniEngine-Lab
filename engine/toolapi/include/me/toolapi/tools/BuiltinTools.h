#pragma once

#include <memory>

#include <nlohmann/json.hpp>

#include "me/core/Transform2D.h"
#include "me/toolapi/ITool.h"

namespace me::domain { struct CalendarTime; }
namespace me::domain { struct CropInstance; class CropDatabase; }

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

/// @brief CalendarTime → JSON { year, season, seasonName, dayOfSeason, minuteOfDay, hour, minute }。
nlohmann::json CalendarToJson(const me::domain::CalendarTime& c);

// —— 时间型 Tool 工厂(M8.1)——
std::unique_ptr<ITool> MakeTimeGetTool();     ///< time.get
std::unique_ptr<ITool> MakeTimeAdvanceTool(); ///< time.advance

/// @brief CropInstance → JSON { x,y,cropId,stage,stageName,daysInStage,watered,mature }。
nlohmann::json CropInstanceToJson(int x, int y, const me::domain::CropInstance& c,
                                  const me::domain::CropDatabase& db);

// —— 作物型 Tool 工厂(M8.2)——
std::unique_ptr<ITool> MakeCropGetFieldTool(); ///< crop.get_field
std::unique_ptr<ITool> MakeCropPlantTool();    ///< crop.plant
std::unique_ptr<ITool> MakeCropWaterTool();    ///< crop.water
std::unique_ptr<ITool> MakeCropAdvanceDaysTool(); ///< crop.advance_days
std::unique_ptr<ITool> MakeCropHarvestTool();  ///< crop.harvest

/// @brief 把 M6 首批全部 Tool 注册进 registry。
void RegisterBuiltinTools(ToolRegistry& registry);

} // namespace me::toolapi
