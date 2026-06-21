#pragma once

#include <string>

#include "me/command/ICommand.h"
#include "me/core/Transform2D.h"
#include "me/scene/Scene.h"

namespace me::command {

/**
 * @brief 设置实体的局部变换;undo 还原首次执行前的旧值。
 *
 * m_captured 守卫确保 redo 不会覆盖首次 execute 前缓存的旧值,
 * 从而保证 undo 总能恢复到命令执行之前的状态。
 */
class SetTransformCmd final : public ICommand {
public:
    /**
     * @brief 构造命令。
     * @param target  要修改的实体持久 ID。
     * @param newLocal 目标局部变换值。
     */
    SetTransformCmd(me::scene::EntityId target, const me::Transform2D& newLocal)
        : m_target(target), m_new(newLocal) {}

    /**
     * @brief 执行(首次)或重做:将实体变换设为 newLocal。
     *        首次调用时缓存旧值以供 undo 使用;redo 不再重新缓存。
     *        若实体已失效则返回 ok=false 且不产生副作用。
     */
    CommandResult execute(me::scene::Scene& scene) override;

    /**
     * @brief 撤销:将实体变换恢复为首次 execute 前的值。
     *        若实体已失效则返回 ok=false。
     */
    CommandResult undo(me::scene::Scene& scene) override;

    /**
     * @brief 返回人读描述,包含目标实体 ID,供 dry-run/日志。
     */
    std::string describe() const override;

private:
    me::scene::EntityId m_target;   ///< 目标实体的持久身份
    me::Transform2D     m_new;      ///< 要设置的新变换
    me::Transform2D     m_old{};    ///< 首次 execute 时缓存的旧变换
    bool                m_captured = false; ///< 是否已缓存旧值(防止 redo 覆盖)
};

} // namespace me::command
