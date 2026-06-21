#pragma once

namespace me::toolapi {

/// @brief 调用者角色,特权按枚举序递增(Agent < Automation < Editor)。
enum class CallerRole {
    Agent,       ///< 未来 LLM/Agent:仅可调用 AgentAllowed
    Automation,  ///< 脚本/自动化测试
    Editor,      ///< 引擎编辑器:可调用全部
};

/// @brief Tool 所需的最低权限,按枚举序限制递增(AgentAllowed < Automation < EditorOnly)。
enum class Permission {
    AgentAllowed, ///< 任意角色可调用(只读或安全变更)
    Automation,   ///< Editor + Automation
    EditorOnly,   ///< 仅 Editor(危险/破坏性)
};

/**
 * @brief 白名单裁决:调用者特权是否 ≥ Tool 要求。
 *
 * 两个枚举均按"越靠后越受限/越高权"排序,故直接整型比较即可:
 * role 的特权值 ≥ required 的限制值时放行。
 */
inline bool IsAllowed(CallerRole role, Permission required) {
    return static_cast<int>(role) >= static_cast<int>(required);
}

} // namespace me::toolapi
