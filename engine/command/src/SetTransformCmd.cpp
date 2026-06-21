#include "me/command/commands/SetTransformCmd.h"

namespace me::command {

CommandResult SetTransformCmd::execute(me::scene::Scene& scene) {
    const me::scene::Entity e = scene.Resolve(m_target);
    if (!e.IsValid()) return CommandResult::Fail("SetTransformCmd 目标实体已失效");

    // 仅首次执行缓存旧值,redo 复用已缓存的值,确保 undo 总能还原首次状态
    if (!m_captured) {
        m_old = scene.LocalTransform(e);
        m_captured = true;
    }

    scene.SetLocalTransform(e, m_new);
    return CommandResult::Ok(describe());
}

CommandResult SetTransformCmd::undo(me::scene::Scene& scene) {
    const me::scene::Entity e = scene.Resolve(m_target);
    if (!e.IsValid()) return CommandResult::Fail("SetTransformCmd::undo 目标实体已失效");
    scene.SetLocalTransform(e, m_old);
    return CommandResult::Ok();
}

std::string SetTransformCmd::describe() const {
    return "设置实体 #" + std::to_string(m_target) + " 局部变换";
}

} // namespace me::command
