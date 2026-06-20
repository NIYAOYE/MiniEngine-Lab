#include "me/scene/Scene.h"

namespace me::scene {

Scene::Slot* Scene::SlotOf(Entity e) {
    if (!e.IsValid() || e.index >= m_slots.size()) return nullptr;
    Slot& s = m_slots[e.index];
    if (!s.alive || s.generation != e.generation) return nullptr;
    return &s;
}

const Scene::Slot* Scene::SlotOf(Entity e) const {
    if (!e.IsValid() || e.index >= m_slots.size()) return nullptr;
    const Slot& s = m_slots[e.index];
    if (!s.alive || s.generation != e.generation) return nullptr;
    return &s;
}

Entity Scene::CreateEntity() {
    std::uint32_t index;
    if (!m_freeList.empty()) {
        index = m_freeList.back();
        m_freeList.pop_back();
    } else {
        index = static_cast<std::uint32_t>(m_slots.size());
        m_slots.emplace_back();
    }
    Slot& s = m_slots[index];
    s.alive = true;
    s.local = me::Transform2D{};
    s.parent = Entity::Invalid();
    s.children.clear();
    s.world = me::Matrix4x4{};
    s.worldDirty = true;
    ++m_aliveCount;
    return Entity{index, s.generation};
}

void Scene::DestroyEntity(Entity e) {
    Slot* s = SlotOf(e);
    if (s == nullptr) return; // 失效句柄:安全无操作

    // 先递归销毁子树(复制子列表,避免遍历中被修改)。
    const std::vector<Entity> kids = s->children;
    for (const Entity child : kids) DestroyEntity(child);

    // 从父的 children 中摘除自己。
    if (Slot* parent = SlotOf(s->parent)) {
        auto& siblings = parent->children;
        for (std::size_t i = 0; i < siblings.size(); ++i) {
            if (siblings[i] == e) {
                siblings[i] = siblings.back();
                siblings.pop_back();
                break;
            }
        }
    }

    // 组件清理钩子(Task 3 实现);此处先声明,Task 3 填充。
    RemoveAllComponents(e);

    s->alive = false;
    s->children.clear();
    s->parent = Entity::Invalid();
    ++s->generation; // 递增代号 → 旧句柄立即失效
    m_freeList.push_back(e.index);
    --m_aliveCount;
}

bool Scene::IsAlive(Entity e) const { return SlotOf(e) != nullptr; }

std::vector<Entity> Scene::AliveEntities() const {
    std::vector<Entity> out;
    out.reserve(m_aliveCount);
    for (std::uint32_t i = 0; i < m_slots.size(); ++i) {
        if (m_slots[i].alive) out.push_back(Entity{i, m_slots[i].generation});
    }
    return out;
}

void Scene::RemoveAllComponents(Entity) { /* Task 3 填充 */ }

} // namespace me::scene
