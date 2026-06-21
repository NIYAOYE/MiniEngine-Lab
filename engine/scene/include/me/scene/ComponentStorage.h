#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#include "me/core/Assert.h"
#include "me/scene/Entity.h"

namespace me::scene {

/// @brief 单个组件的只读快照:可把所记录的组件值重新写回某实体(供命令 Undo 还原)。
class IComponentSnapshot {
public:
    virtual ~IComponentSnapshot() = default;
    /// @brief 把快照里的组件值添加到实体 e(写回到产生本快照的存储)。
    virtual void RestoreTo(Entity e) const = 0;
};

template <class T>
class ComponentStorage; // 前置声明,供 Capture 返回 ComponentSnapshot<T>

/// @brief 类型擦除的组件存储接口:Scene 据此在销毁实体时统一移除其组件。
class IComponentStorage {
public:
    virtual ~IComponentStorage() = default;
    /// @brief 移除某实体的该类组件(无则无操作)。
    virtual void Remove(Entity e) = 0;
    /// @brief 该实体是否拥有此类组件。
    virtual bool Has(Entity e) const = 0;
    /// @brief 若实体拥有该组件,返回其快照;否则返回 nullptr。
    virtual std::unique_ptr<IComponentSnapshot> Capture(Entity e) = 0;
};

/**
 * @brief 稠密存储的组件容器(sparse-set 思路):dense 数组连续,删除用 swap-pop。
 *
 * 以 Entity.index 为稀疏键映射到 dense 下标;Entities()/Items() 并行,供 System
 * 顺序遍历(缓存友好)。组件为纯数据 T。
 */
template <class T>
class ComponentStorage final : public IComponentStorage {
public:
    /// @brief 新增/覆盖某实体的组件,返回其引用。
    T& Add(Entity e, const T& value) {
        auto it = m_sparse.find(e.index);
        if (it != m_sparse.end()) {
            m_items[it->second] = value;
            m_owners[it->second] = e;
            return m_items[it->second];
        }
        const std::size_t dense = m_items.size();
        m_items.push_back(value);
        m_owners.push_back(e);
        m_sparse.emplace(e.index, dense);
        return m_items.back();
    }

    /// @brief 取组件指针;无则 nullptr。
    T* Get(Entity e) {
        auto it = m_sparse.find(e.index);
        if (it == m_sparse.end() || m_owners[it->second] != e) return nullptr;
        return &m_items[it->second];
    }

    bool Has(Entity e) const override {
        auto it = m_sparse.find(e.index);
        return it != m_sparse.end() && m_owners[it->second] == e;
    }

    void Remove(Entity e) override {
        auto it = m_sparse.find(e.index);
        // 同 Get/Has:比对完整 Entity(含 generation),拒绝已回收槽位的悬垂句柄。
        if (it == m_sparse.end() || m_owners[it->second] != e) return;
        const std::size_t dense = it->second;
        const std::size_t last = m_items.size() - 1;
        if (dense != last) {
            // 用末尾元素填洞,并修正其稀疏映射。
            m_items[dense] = m_items[last];
            m_owners[dense] = m_owners[last];
            m_sparse[m_owners[dense].index] = dense;
        }
        m_items.pop_back();
        m_owners.pop_back();
        m_sparse.erase(it);
    }

    std::unique_ptr<IComponentSnapshot> Capture(Entity e) override;

    /// @brief 拥有该组件的实体(与 Items() 并行,供 System 遍历)。
    const std::vector<Entity>& Entities() const { return m_owners; }
    /// @brief 组件数据(与 Entities() 并行)。
    std::vector<T>& Items() { return m_items; }
    const std::vector<T>& Items() const { return m_items; }
    std::size_t Size() const { return m_items.size(); }

private:
    std::vector<T> m_items;                              // dense 组件
    std::vector<Entity> m_owners;                        // dense 拥有者(并行)
    std::unordered_map<std::uint32_t, std::size_t> m_sparse; // entity.index → dense
};

/// @brief 持有某组件值拷贝的快照;RestoreTo 时写回创建它的存储实例。
template <class T>
class ComponentSnapshot final : public IComponentSnapshot {
public:
    ComponentSnapshot(ComponentStorage<T>* store, const T& value)
        : m_store(store), m_value(value) {}
    void RestoreTo(Entity e) const override { m_store->Add(e, m_value); }

private:
    ComponentStorage<T>* m_store; // 非拥有:存储归 Scene,生命周期长于快照
    T m_value;
};

template <class T>
std::unique_ptr<IComponentSnapshot> ComponentStorage<T>::Capture(Entity e) {
    auto it = m_sparse.find(e.index);
    if (it == m_sparse.end() || m_owners[it->second] != e) return nullptr;
    return std::make_unique<ComponentSnapshot<T>>(this, m_items[it->second]);
}

} // namespace me::scene
