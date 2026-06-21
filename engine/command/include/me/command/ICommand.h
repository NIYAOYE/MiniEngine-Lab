#pragma once

#include <string>

#include "me/command/CommandResult.h"

namespace me::scene {
class Scene;
}

namespace me::command {

/**
 * @brief 可回滚命令接口。
 *
 * execute 用于首次执行与 redo(二者语义一致);undo 撤销;describe 供 dry-run 预览。
 * 实现者承诺:undo 能把场景恢复到 execute 之前的等价状态。
 */
class ICommand {
public:
    virtual ~ICommand() = default;
    /// @brief 执行(或重做)。失败返回 ok=false 且不应产生部分副作用。
    virtual CommandResult execute(me::scene::Scene& scene) = 0;
    /// @brief 撤销最近一次 execute。
    virtual CommandResult undo(me::scene::Scene& scene) = 0;
    /// @brief 返回人读的命令描述,供 dry-run/日志。
    virtual std::string describe() const = 0;
};

} // namespace me::command
