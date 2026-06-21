#pragma once

#include <string>

#include "me/command/ICommand.h"
#include "me/scene/Scene.h"

namespace me::command {

/**
 * @brief 创建一个新实体(局部变换为单位、无父)。
 *
 * execute 首次创建并记录其 EntityId;redo 以同一 EntityId 重建;undo 销毁。
 */
class CreateEntityCmd final : public ICommand {
public:
    CommandResult execute(me::scene::Scene& scene) override;
    CommandResult undo(me::scene::Scene& scene) override;
    std::string describe() const override;

    /// @brief 已创建实体的持久身份(execute 后有效;未执行为 0)。
    me::scene::EntityId CreatedId() const { return m_id; }

private:
    me::scene::EntityId m_id = 0; // 0 = 尚未首次执行
};

} // namespace me::command
