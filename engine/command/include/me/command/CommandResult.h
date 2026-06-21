#pragma once

#include <string>
#include <utility>

namespace me::command {

/**
 * @brief 命令执行结果(不抛异常)。可恢复失败以 ok=false + message 返回。
 *
 * M6 会把它包装为 JSON 边界的 ToolResult;M5 阶段仅人读消息。
 */
struct CommandResult {
    bool ok = true;
    std::string message;

    /// @brief 构造成功结果(可附带说明)。
    static CommandResult Ok(std::string msg = {}) { return {true, std::move(msg)}; }
    /// @brief 构造失败结果(必须给出原因)。
    static CommandResult Fail(std::string msg) { return {false, std::move(msg)}; }
};

} // namespace me::command
