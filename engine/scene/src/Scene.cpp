#include "me/scene/Scene.h"

#include "me/core/Assert.h"

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
    s.id = m_nextId++;
    m_idToIndex.emplace(s.id, index);
    ++m_aliveCount;
    return Entity{index, s.generation};
}

void Scene::DestroyEntity(Entity e) {
    Slot* s = SlotOf(e);
    if (s == nullptr) return; // 失效句柄:安全无操作

    // 若被销毁实体正是活动相机,立即清除,防止 m_activeCamera 残留悬垂句柄。
    if (m_activeCamera == e) m_activeCamera = Entity::Invalid();

    // 先递归销毁子树(复制子列表,避免遍历中被修改)。
    const std::vector<Entity> kids = s->children;
    const std::uint32_t index = e.index;
    for (const Entity child : kids) DestroyEntity(child);

    // 递归销毁子树后按 index 重取槽位:子树销毁不会增长 m_slots,但显式重取避免依赖该隐式不变量。
    Slot& slot = m_slots[index];

    // 从父的 children 中摘除自己。
    if (Slot* parent = SlotOf(slot.parent)) {
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

    m_idToIndex.erase(slot.id);
    slot.id = 0;
    slot.alive = false;
    slot.children.clear();
    slot.parent = Entity::Invalid();
    ++slot.generation; // 递增代号 → 旧句柄立即失效
    m_freeList.push_back(index);
    --m_aliveCount;
}

bool Scene::IsAlive(Entity e) const { return SlotOf(e) != nullptr; }

EntityId Scene::IdOf(Entity e) const {
    const Slot* s = SlotOf(e);
    return s ? s->id : 0;
}

Entity Scene::Resolve(EntityId id) const {
    if (id == 0) return Entity::Invalid();
    auto it = m_idToIndex.find(id);
    if (it == m_idToIndex.end()) return Entity::Invalid();
    const std::uint32_t index = it->second;
    return Entity{index, m_slots[index].generation};
}

Entity Scene::CreateEntityWithId(EntityId id) {
    ME_ASSERT_MSG(id != 0, "CreateEntityWithId: id 不能为 0");
    ME_ASSERT_MSG(m_idToIndex.find(id) == m_idToIndex.end(),
                  "CreateEntityWithId: id 当前已存活");
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
    s.id = id;
    m_idToIndex.emplace(id, index);
    if (id >= m_nextId) m_nextId = id + 1; // 保证后续分配不与恢复的 id 冲突
    ++m_aliveCount;
    return Entity{index, s.generation};
}

std::vector<Entity> Scene::AliveEntities() const {
    std::vector<Entity> out;
    out.reserve(m_aliveCount);
    for (std::uint32_t i = 0; i < m_slots.size(); ++i) {
        if (m_slots[i].alive) out.push_back(Entity{i, m_slots[i].generation});
    }
    return out;
}

void Scene::RemoveAllComponents(Entity e) {
    for (auto& kv : m_stores) kv.second->Remove(e);
}

std::vector<std::unique_ptr<IComponentSnapshot>> Scene::CaptureComponents(Entity e) {
    ME_ASSERT_MSG(IsAlive(e), "CaptureComponents: 实体已失效");
    std::vector<std::unique_ptr<IComponentSnapshot>> out;
    for (auto& kv : m_stores) {
        if (auto snap = kv.second->Capture(e)) out.push_back(std::move(snap));
    }
    return out;
}

void Scene::RestoreComponents(
    Entity e, const std::vector<std::unique_ptr<IComponentSnapshot>>& snaps) {
    // 空指针守护:契约上不应出现 null snap,但防御性跳过避免静默解引用 UB。
    for (const auto& snap : snaps) { if (snap) snap->RestoreTo(e); }
}

void Scene::SetLocalTransform(Entity e, const me::Transform2D& t) {
    Slot* s = SlotOf(e);
    ME_ASSERT_MSG(s != nullptr, "SetLocalTransform: 实体已失效");
    s->local = t;
    MarkSubtreeDirty(e);
}

const me::Transform2D& Scene::LocalTransform(Entity e) const {
    const Slot* s = SlotOf(e);
    ME_ASSERT_MSG(s != nullptr, "LocalTransform: 实体已失效");
    return s->local;
}

void Scene::SetParent(Entity child, Entity parent) {
    Slot* cs = SlotOf(child);
    ME_ASSERT_MSG(cs != nullptr, "SetParent: child 已失效");
    if (parent.IsValid()) {
        ME_ASSERT_MSG(SlotOf(parent) != nullptr, "SetParent: parent 已失效");
        ME_ASSERT_MSG(child != parent, "SetParent: 不能以自身为父");
        ME_ASSERT_MSG(!IsDescendantOf(parent, child),
                      "SetParent: 会形成环路(parent 是 child 的后代)");
    }
    // 从旧父摘除。
    if (Slot* oldParent = SlotOf(cs->parent)) {
        auto& sib = oldParent->children;
        for (std::size_t i = 0; i < sib.size(); ++i) {
            if (sib[i] == child) { sib[i] = sib.back(); sib.pop_back(); break; }
        }
    }
    cs->parent = parent;
    if (Slot* np = SlotOf(parent)) np->children.push_back(child);
    MarkSubtreeDirty(child);
}

Entity Scene::Parent(Entity e) const {
    const Slot* s = SlotOf(e);
    ME_ASSERT_MSG(s != nullptr, "Parent: 实体已失效");
    return s->parent;
}

const std::vector<Entity>& Scene::Children(Entity e) const {
    const Slot* s = SlotOf(e);
    ME_ASSERT_MSG(s != nullptr, "Children: 实体已失效");
    return s->children;
}

bool Scene::IsWorldDirty(Entity e) const {
    const Slot* s = SlotOf(e);
    ME_ASSERT_MSG(s != nullptr, "IsWorldDirty: 实体已失效");
    return s->worldDirty;
}

void Scene::MarkSubtreeDirty(Entity e) {
    Slot* s = SlotOf(e);
    if (s == nullptr) return;
    s->worldDirty = true;
    for (const Entity child : s->children) MarkSubtreeDirty(child);
}

bool Scene::IsDescendantOf(Entity e, Entity maybeAncestor) const {
    // 从 e 的父开始上行(不含 e 自身:节点不是自身的后代);
    // 链上若遇到 maybeAncestor 则 e 是其(真)后代。
    const Slot* s = SlotOf(e);
    Entity cur = s ? s->parent : Entity::Invalid();
    while (cur.IsValid()) {
        if (cur == maybeAncestor) return true;
        const Slot* cs = SlotOf(cur);
        if (cs == nullptr) break;
        cur = cs->parent;
    }
    return false;
}

const me::Matrix4x4& Scene::WorldMatrix(Entity e) {
    Slot* s = SlotOf(e);
    ME_ASSERT_MSG(s != nullptr, "WorldMatrix: 实体已失效");
    if (!s->worldDirty) return s->world;
    const me::Matrix4x4 localM = s->local.ToMatrix();
    if (s->parent.IsValid()) {
        // 先递归解析父(父先于子),再 world = local * parentWorld(行向量约定)。
        const me::Matrix4x4 parentWorld = WorldMatrix(s->parent); // 拷贝,避免下行使 s 失效后引用悬垂
        s = SlotOf(e); // 递归可能引起 m_slots 重分配?否(本任务不增删槽位);仍重取以稳健
        s->world = localM * parentWorld;
    } else {
        s->world = localM;
    }
    s->worldDirty = false;
    return s->world;
}

} // namespace me::scene
