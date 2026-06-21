#include "me/toolapi/tools/BuiltinTools.h"

#include "me/toolapi/ToolRegistry.h"

namespace me::toolapi {

void RegisterBuiltinTools(ToolRegistry& registry) {
    // 查询型
    registry.Register(MakeListEntitiesTool());
    registry.Register(MakeGetEntityTool());
    // TODO(Task 7): registry.Register(MakeLogReadTool());
    // 变更型
    // TODO(Task 7/8): registry.Register(MakeCreateEntityTool());
    // TODO(Task 7/8): registry.Register(MakeDestroyEntityTool());
    // TODO(Task 7/8): registry.Register(MakeSetTransformTool());
}

} // namespace me::toolapi
