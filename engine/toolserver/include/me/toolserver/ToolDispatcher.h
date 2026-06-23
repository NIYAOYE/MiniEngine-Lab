#pragma once

#include <mutex>
#include <string>

namespace me::toolapi { struct ToolContext; class ToolRegistry; }

namespace me::toolserver {

/**
 * @brief 纯逻辑 Tool 调度核心:JSON 字符串入,JSON 字符串出,不含 socket。
 *
 * 线程安全:内部互斥锁串行化每次调用(共享 Scene/Farm 状态需原子)。
 * HandleInvoke 对任何非法输入都返回结构化 {ok:false,code} 的 JSON,绝不抛异常。
 */
class ToolDispatcher {
public:
    /// @brief 注入受控上下文与已注册 Builtin Tool 的 registry。
    ToolDispatcher(me::toolapi::ToolContext& ctx, me::toolapi::ToolRegistry& registry);

    /// @brief 处理一次 /invoke:解析 body → Invoke → 序列化 ToolResult 为 JSON 字符串。
    /// @param jsonBody 形如 {"name":..,"params":{..}?,"role":..?,"dryRun":..?}。
    std::string HandleInvoke(const std::string& jsonBody);

    /// @brief 处理 /tools:返回 [{name,category,permission,paramsSchema}] 的 JSON 字符串。
    std::string HandleListTools();

private:
    me::toolapi::ToolContext& ctx_;
    me::toolapi::ToolRegistry& registry_;
    std::mutex mutex_; ///< 串行化 Invoke/ListTools(共享 Scene/Farm 状态)
};

} // namespace me::toolserver
