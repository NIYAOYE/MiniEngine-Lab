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
}

} // namespace me::toolapi
