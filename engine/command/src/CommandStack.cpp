#include "me/command/CommandStack.h"

namespace me::command {

CommandResult CommandStack::execute(std::unique_ptr<ICommand> cmd,
                                    me::scene::Scene& scene) {
    CommandResult r = cmd->execute(scene);
    if (!r.ok) return r;          // 失败命令不入栈,无副作用承诺由命令保证
    m_undo.push_back(std::move(cmd));
    m_redo.clear();               // 新分支:历史 redo 失效
    return r;
}

CommandResult CommandStack::undo(me::scene::Scene& scene) {
    if (m_undo.empty()) return CommandResult::Fail("undo 栈为空");
    // 先执行再移动:失败则命令留在 undo 栈,便于诊断。
    CommandResult r = m_undo.back()->undo(scene);
    if (!r.ok) return r;
    m_redo.push_back(std::move(m_undo.back()));
    m_undo.pop_back();
    return r;
}

CommandResult CommandStack::redo(me::scene::Scene& scene) {
    if (m_redo.empty()) return CommandResult::Fail("redo 栈为空");
    CommandResult r = m_redo.back()->execute(scene);
    if (!r.ok) return r;
    m_undo.push_back(std::move(m_redo.back()));
    m_redo.pop_back();
    return r;
}

void CommandStack::clear() {
    m_undo.clear();
    m_redo.clear();
}

} // namespace me::command
