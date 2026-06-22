#include "me/toolapi/tools/BuiltinTools.h"

#include "me/toolapi/ToolRegistry.h"

namespace me::toolapi {

void RegisterBuiltinTools(ToolRegistry& registry) {
    // 查询型
    registry.Register(MakeListEntitiesTool());
    registry.Register(MakeGetEntityTool());
    registry.Register(MakeLogReadTool());
    // 变更型
    registry.Register(MakeCreateEntityTool());
    registry.Register(MakeDestroyEntityTool());
    registry.Register(MakeSetTransformTool());
    // 时间型(M8.1)
    registry.Register(MakeTimeGetTool());
    registry.Register(MakeTimeAdvanceTool());
    // 作物型(M8.2)
    registry.Register(MakeCropGetFieldTool());
    registry.Register(MakeCropPlantTool());
    registry.Register(MakeCropWaterTool());
}

} // namespace me::toolapi
