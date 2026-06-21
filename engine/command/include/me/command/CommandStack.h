#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include "me/command/CommandResult.h"
#include "me/command/ICommand.h"

namespace me::scene {
class Scene;
}

namespace me::command {

/**
 * @brief 标准 Undo/Redo 双栈。
 *
 * 语义:execute 成功才压入 undo 栈并清空 redo 栈;undo/redo 在两栈间移动。
 * 不做命令合并、不设容量上限(YAGNI,留待交互编辑器里程碑)。
 */
class CommandStack {
public:
    /// @brief 执行命令;成功则接管所有权入 undo 栈并清空 redo;失败则丢弃命令、不入栈。
    CommandResult execute(std::unique_ptr<ICommand> cmd, me::scene::Scene& scene);
    /// @brief 撤销栈顶命令;undo 栈空返回失败。
    CommandResult undo(me::scene::Scene& scene);
    /// @brief 重做最近撤销的命令;redo 栈空返回失败。
    CommandResult redo(me::scene::Scene& scene);

    /// @brief 是否有可撤销的命令。
    bool canUndo() const { return !m_undo.empty(); }
    /// @brief 是否有可重做的命令。
    bool canRedo() const { return !m_redo.empty(); }
    /// @brief 撤销栈深度。
    std::size_t undoDepth() const { return m_undo.size(); }
    /// @brief 重做栈深度。
    std::size_t redoDepth() const { return m_redo.size(); }
    /// @brief 清空两栈(丢弃全部历史)。
    void clear();

private:
    std::vector<std::unique_ptr<ICommand>> m_undo;
    std::vector<std::unique_ptr<ICommand>> m_redo;
};

} // namespace me::command
