#include "me/command/commands/CreateEntityCmd.h"

namespace me::command {

CommandResult CreateEntityCmd::execute(me::scene::Scene& scene) {
    if (m_id == 0) {
        // 首次执行:分配新实体并记录其持久身份。
        const me::scene::Entity e = scene.CreateEntity();
        m_id = scene.IdOf(e);
    } else {
        // redo:以原身份重建,保证后续命令对该 id 的引用仍然成立。
        scene.CreateEntityWithId(m_id);
    }
    return CommandResult::Ok("创建实体 #" + std::to_string(m_id));
}

CommandResult CreateEntityCmd::undo(me::scene::Scene& scene) {
    const me::scene::Entity e = scene.Resolve(m_id);
    if (!e.IsValid()) return CommandResult::Fail("CreateEntityCmd::undo 实体已失效");
    scene.DestroyEntity(e);
    return CommandResult::Ok();
}

std::string CreateEntityCmd::describe() const {
    return "创建实体 #" + std::to_string(m_id);
}

} // namespace me::command
