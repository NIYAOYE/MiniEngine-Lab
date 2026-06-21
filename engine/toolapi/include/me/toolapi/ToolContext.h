#pragma once

#include "me/command/CommandStack.h"
#include "me/scene/Scene.h"
#include "me/toolapi/ToolInvocation.h"

namespace me::toolapi {

/**
 * @brief 注入给 Tool 的受控门面:Tool 只能经此访问被授权的子系统。
 *
 * 不持有全局指针(契合禁 Singleton / 禁全局可变状态)。变更型 Tool 经 commands
 * 落地以获得 Undo;查询型 Tool 直接读 scene;log.read 读 log。
 */
struct ToolContext {
    me::scene::Scene& scene;            ///< 受控场景访问
    me::command::CommandStack& commands;///< 变更型 Tool 的唯一落地通道
    ToolInvocationLog& log;             ///< 审计日志(log.read 数据源)
};

} // namespace me::toolapi
