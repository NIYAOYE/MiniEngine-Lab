#pragma once

#include <cstdint>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

namespace me::toolapi {

/// @brief Tool 统一错误码(不抛异常;所有失败结构化返回)。
enum class ToolErrorCode {
    Ok,
    UnknownTool,       ///< Registry 中无此 Tool
    PermissionDenied,  ///< 调用者角色不在该 Tool 白名单
    InvalidParams,     ///< 参数未通过 JSON Schema 校验
    PreconditionFailed,///< 前置条件不满足(如实体不存在)
    ExecutionFailed,   ///< 执行阶段失败(如 Command 返回 ok=false)
};

/// @brief 错误码 → 稳定字符串(用于 JSON 边界与日志)。
const char* ToString(ToolErrorCode code);

/**
 * @brief Tool 调用结果(JSON 边界)。可恢复失败以 ok=false + code + message 返回。
 *
 * data 承载成功载荷或失败明细(如 InvalidParams 的 errors 列表)。
 */
struct ToolResult {
    bool ok = true;
    ToolErrorCode code = ToolErrorCode::Ok;
    std::string message;
    nlohmann::json data = nlohmann::json::object();
    std::uint64_t invocationId = 0; ///< 由 Registry 记录调用后回填

    /// @brief 构造成功结果。
    static ToolResult Success(nlohmann::json data = nlohmann::json::object(),
                              std::string msg = {}) {
        ToolResult r;
        r.ok = true;
        r.code = ToolErrorCode::Ok;
        r.message = std::move(msg);
        r.data = std::move(data);
        return r;
    }

    /// @brief 构造失败结果(必须给出错误码与原因)。
    static ToolResult Error(ToolErrorCode code, std::string msg,
                            nlohmann::json data = nlohmann::json::object()) {
        ToolResult r;
        r.ok = false;
        r.code = code;
        r.message = std::move(msg);
        r.data = std::move(data);
        return r;
    }

    /// @brief 序列化为统一 JSON 形态 { ok, code, message, data, invocationId }。
    nlohmann::json toJson() const;
};

} // namespace me::toolapi
