#pragma once

#include "me/command/CommandStack.h"
#include "me/scene/Scene.h"
#include "me/toolapi/ToolInvocation.h"

namespace me::domain { class TimeSystem; } // 前置声明:仅按指针使用,避免拉入 domain 头
namespace me::domain { class FarmField; }   // 前置声明:仅按指针使用
namespace me::domain { class Inventory; }   // 前置声明:仅按指针使用

namespace me::toolapi {

/**
 * @brief 注入给 Tool 的受控门面:Tool 只能经此访问被授权的子系统。
 *
 * 不持有全局指针(契合禁 Singleton / 禁全局可变状态)。变更型 Tool 经 commands
 * 落地以获得 Undo;查询型 Tool 直接读 scene;log.read 读 log;时间 Tool 读 time。
 */
struct ToolContext {
    me::scene::Scene& scene;            ///< 受控场景访问
    me::command::CommandStack& commands;///< 变更型 Tool 的唯一落地通道
    ToolInvocationLog& log;             ///< 审计日志(log.read 数据源)
    me::domain::TimeSystem* time = nullptr; ///< 可选:时间 Tool 数据源,缺省 nullptr
    me::domain::FarmField* farm = nullptr;   ///< 可选:crop Tool 数据源,缺省 nullptr
    me::domain::Inventory* inventory = nullptr; ///< 可选:inventory Tool 数据源,缺省 nullptr
};

} // namespace me::toolapi
