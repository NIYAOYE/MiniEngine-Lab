#pragma once

#include <memory>
#include <string>
#include <vector>

#include "me/command/ICommand.h"
#include "me/core/Transform2D.h"
#include "me/scene/ComponentStorage.h"
#include "me/scene/Scene.h"

namespace me::command {

/**
 * @brief 销毁实体及其整棵子树;undo 按原 EntityId 还原层级/变换/组件/active camera。
 *
 * execute 先前序快照子树(父先于子),再销毁;每次 execute 都刷新快照,
 * 因此 redo→undo 仍一致。
 */
class DestroyEntityCmd final : public ICommand {
public:
    explicit DestroyEntityCmd(me::scene::EntityId target) : m_target(target) {}

    CommandResult execute(me::scene::Scene& scene) override;
    CommandResult undo(me::scene::Scene& scene) override;
    std::string describe() const override;

private:
    // 单个实体的完整快照(身份 + 局部变换 + 父身份 + 组件)。
    struct NodeSnapshot {
        me::scene::EntityId id = 0;
        me::scene::EntityId parentId = 0; // 0 = 无父
        me::Transform2D local{};
        std::vector<std::unique_ptr<me::scene::IComponentSnapshot>> comps;
    };

    me::scene::EntityId m_target;
    std::vector<NodeSnapshot> m_nodes;        // 前序:父先于子
    me::scene::EntityId m_activeCameraId = 0; // 被销毁子树内的 active camera(0 = 不在子树内)
};

} // namespace me::command
