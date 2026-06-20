#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "me/core/Matrix4x4.h"
#include "me/core/Transform2D.h"
#include "me/scene/Entity.h"

namespace me::scene {

/**
 * @brief 混合实体模型的场景容器(纯 CPU,不持有 RHI 资源,可独立单测)。
 *
 * 持有每个实体的存活/代号、局部 Transform2D、父子邻接、缓存世界矩阵与脏标记。
 * 组件存储在后续任务以模板 API 加入。System(TransformSystem/RenderSystem)以
 * 显式传入 Scene& 的方式处理,不引入全局状态。
 */
class Scene {
public:
    /// @brief 新建一个实体(局部变换为单位、无父)。复用空闲槽位。
    Entity CreateEntity();

    /// @brief 销毁实体及其整棵子树与全部组件;之后旧句柄 IsAlive==false。
    void DestroyEntity(Entity e);

    /// @brief 句柄是否指向当前存活实体(index 在范围内且 generation 匹配)。
    bool IsAlive(Entity e) const;

    /// @brief 当前存活实体数量。
    std::size_t AliveCount() const { return m_aliveCount; }

    /// @brief 收集全部存活实体(顺序为槽位顺序,供 System 遍历)。
    std::vector<Entity> AliveEntities() const;

private:
    // 每实体一个槽位;index 即句柄 index。销毁后 alive=false 并加入空闲表。
    struct Slot {
        std::uint32_t generation = 0;
        bool alive = false;
        me::Transform2D local{};
        Entity parent = Entity::Invalid();
        std::vector<Entity> children;
        me::Matrix4x4 world{};         // 缓存世界矩阵(TransformSystem 解析)
        bool worldDirty = true;
    };

    // 校验句柄有效并返回槽位指针;无效返回 nullptr(不触发断言,供查询型 API)。
    Slot* SlotOf(Entity e);
    const Slot* SlotOf(Entity e) const;

    // 销毁实体时移除其全部组件(Task 3 填充存储遍历)。
    void RemoveAllComponents(Entity e);

    std::vector<Slot> m_slots;
    std::vector<std::uint32_t> m_freeList; // 可复用槽位 index
    std::size_t m_aliveCount = 0;

    // —— 后续任务在此追加:层级、世界矩阵解析、组件存储 ——
};

} // namespace me::scene
