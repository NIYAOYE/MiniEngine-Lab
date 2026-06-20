#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <vector>

#include "me/core/Assert.h"
#include "me/core/Matrix4x4.h"
#include "me/core/Transform2D.h"
#include "me/scene/ComponentStorage.h"
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

    // —— 层级与变换 ——
    /// @brief 设置实体局部变换,并把以它为根的子树标记为世界脏。
    void SetLocalTransform(Entity e, const me::Transform2D& t);
    /// @brief 读取实体局部变换(实体须存活)。
    const me::Transform2D& LocalTransform(Entity e) const;
    /// @brief 设置父实体(传 Entity::Invalid() 脱离);更新邻接并把子树标记脏。
    void SetParent(Entity child, Entity parent);
    /// @brief 父实体句柄(无父时为 Invalid)。
    Entity Parent(Entity e) const;
    /// @brief 子实体列表(实体须存活;无子时为空)。
    const std::vector<Entity>& Children(Entity e) const;
    /// @brief 解析并返回世界矩阵(惰性:脏则沿父链重算并缓存)。
    const me::Matrix4x4& WorldMatrix(Entity e);
    /// @brief 世界矩阵是否待重算。
    bool IsWorldDirty(Entity e) const;

    // —— 组件(数据型,存储隐藏在 ComponentStorage 接口后)——
    /// @brief 给实体添加/覆盖组件,返回引用。实体须存活。
    template <class T>
    T& AddComponent(Entity e, const T& value = T{}) {
        ME_ASSERT_MSG(IsAlive(e), "AddComponent: 实体已失效");
        return ComponentStore<T>().Add(e, value);
    }

    /// @brief 取组件指针;无则 nullptr。
    template <class T>
    T* GetComponent(Entity e) {
        auto* store = FindStore<T>();
        return store ? store->Get(e) : nullptr;
    }

    /// @brief 实体是否拥有该类组件。
    template <class T>
    bool HasComponent(Entity e) const {
        const auto* store = FindStore<T>();
        return store && store->Has(e);
    }

    /// @brief 移除组件(无则无操作)。
    template <class T>
    void RemoveComponent(Entity e) {
        if (auto* store = FindStore<T>()) store->Remove(e);
    }

    /// @brief 取(必要时创建)某类型的组件存储,供 System 顺序遍历。
    template <class T>
    ComponentStorage<T>& ComponentStore() {
        const std::type_index key(typeid(T));
        auto it = m_stores.find(key);
        if (it == m_stores.end()) {
            auto store = std::make_unique<ComponentStorage<T>>();
            ComponentStorage<T>& ref = *store;
            m_stores.emplace(key, std::move(store));
            return ref;
        }
        return *static_cast<ComponentStorage<T>*>(it->second.get());
    }

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
    // 把以 e 为根的子树全部标记为世界脏(局部/父变更后调用)。
    void MarkSubtreeDirty(Entity e);
    // e 是否为 maybeAncestor 的(传递)后代;用于 SetParent 环路防护。
    bool IsDescendantOf(Entity e, Entity maybeAncestor) const;

    std::vector<Slot> m_slots;
    std::vector<std::uint32_t> m_freeList; // 可复用槽位 index
    std::size_t m_aliveCount = 0;

    // 类型擦除的组件存储集合(每种组件类型一个 ComponentStorage<T>)。
    std::unordered_map<std::type_index, std::unique_ptr<IComponentStorage>> m_stores;

    // 查找已存在的存储(不创建);const 与非 const 重载。
    template <class T>
    ComponentStorage<T>* FindStore() {
        auto it = m_stores.find(std::type_index(typeid(T)));
        return it == m_stores.end()
                   ? nullptr
                   : static_cast<ComponentStorage<T>*>(it->second.get());
    }
    template <class T>
    const ComponentStorage<T>* FindStore() const {
        auto it = m_stores.find(std::type_index(typeid(T)));
        return it == m_stores.end()
                   ? nullptr
                   : static_cast<const ComponentStorage<T>*>(it->second.get());
    }
};

} // namespace me::scene
