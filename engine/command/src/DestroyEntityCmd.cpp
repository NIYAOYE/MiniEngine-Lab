#include "me/command/commands/DestroyEntityCmd.h"

#include <vector>

namespace me::command {

CommandResult DestroyEntityCmd::execute(me::scene::Scene& scene) {
    const me::scene::Entity root = scene.Resolve(m_target);
    if (!root.IsValid()) return CommandResult::Fail("DestroyEntityCmd 目标实体已失效");

    m_nodes.clear();
    m_activeCameraId = 0;
    const me::scene::Entity activeCam = scene.ActiveCamera();
    const me::scene::EntityId activeCamId = scene.IdOf(activeCam);

    // 前序栈遍历:父先于子入列(保证 undo 时父先重建)。
    std::vector<me::scene::Entity> stack{root};
    while (!stack.empty()) {
        const me::scene::Entity cur = stack.back();
        stack.pop_back();
        NodeSnapshot node;
        node.id = scene.IdOf(cur);
        node.parentId = scene.IdOf(scene.Parent(cur));
        node.local = scene.LocalTransform(cur);
        node.comps = scene.CaptureComponents(cur);
        if (activeCamId != 0 && node.id == activeCamId) m_activeCameraId = activeCamId;
        m_nodes.push_back(std::move(node));
        // 逆序压栈使弹出顺序与 children 顺序一致(前序稳定)。
        const auto& kids = scene.Children(cur);
        for (auto it = kids.rbegin(); it != kids.rend(); ++it) stack.push_back(*it);
    }

    // active camera 在子树内时,销毁前先置空,避免场景持有悬垂句柄。
    if (m_activeCameraId != 0) scene.SetActiveCamera(me::scene::Entity::Invalid());
    scene.DestroyEntity(root); // 连带销毁整棵子树
    return CommandResult::Ok(describe());
}

CommandResult DestroyEntityCmd::undo(me::scene::Scene& scene) {
    // 父先于子:逐个以原 id 重建,再连父子、还原变换与组件。
    for (auto& node : m_nodes) {
        const me::scene::Entity e = scene.CreateEntityWithId(node.id);
        if (node.parentId != 0) {
            const me::scene::Entity parent = scene.Resolve(node.parentId);
            if (parent.IsValid()) scene.SetParent(e, parent);
        }
        scene.SetLocalTransform(e, node.local);
        scene.RestoreComponents(e, node.comps);
    }
    if (m_activeCameraId != 0) {
        const me::scene::Entity cam = scene.Resolve(m_activeCameraId);
        if (cam.IsValid()) scene.SetActiveCamera(cam);
    }
    return CommandResult::Ok();
}

std::string DestroyEntityCmd::describe() const {
    return "销毁实体 #" + std::to_string(m_target) + " 及其子树(" +
           std::to_string(m_nodes.size()) + " 个实体)";
}

} // namespace me::command
