#include "me/toolapi/ToolResult.h"

namespace me::toolapi {

const char* ToString(ToolErrorCode code) {
    switch (code) {
        case ToolErrorCode::Ok:                 return "Ok";
        case ToolErrorCode::UnknownTool:        return "UnknownTool";
        case ToolErrorCode::PermissionDenied:   return "PermissionDenied";
        case ToolErrorCode::InvalidParams:      return "InvalidParams";
        case ToolErrorCode::PreconditionFailed: return "PreconditionFailed";
        case ToolErrorCode::ExecutionFailed:    return "ExecutionFailed";
    }
    return "Ok"; // 不可达;穷尽 switch 后的默认
}

nlohmann::json ToolResult::toJson() const {
    return nlohmann::json{
        {"ok", ok},
        {"code", ToString(code)},
        {"message", message},
        {"data", data},
        {"invocationId", invocationId},
    };
}

} // namespace me::toolapi
