#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "me/core/Assert.h"
#include "me/scene/Entity.h"

namespace me::scene {

/// @brief 类型擦除的组件存储接口:Scene 据此在销毁实体时统一移除其组件。
class IComponentStorage {
public:
    virtual ~IComponentStorage() = default;
    /// @brief 移除某实体的该类组件(无则无操作)。
    virtual void Remove(Entity e) = 0;
    /// @brief 该实体是否拥有此类组件。
    virtual bool Has(Entity e) const = 0;
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
        if (it == m_sparse.end()) return;
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

} // namespace me::scene
