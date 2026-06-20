# M4 Scene + 组件 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 引入混合实体模型的 `me_scene` 层 —— `Entity`/`Scene` + `Transform2D` 父子层级 + 数据型 Component(Sprite/Camera/TileMap)+ System(`TransformSystem` 解析世界矩阵、`RenderSystem` 产出按层/Y 排序的 `RenderView`),全部纯 CPU、可在 WSL 用 doctest 红绿。

**Architecture:** `me_scene` 是 CPU-only 静态库(**始终构建**,非 WIN32 限定),仅依赖 `me_core` + `me_assets`,**不碰 RHI/Renderer**。`Entity = me::Handle<EntityTag>`(复用 Core 的 index+generation 句柄,带槽位回收)。Scene 持有每实体的局部 `Transform2D`、父/子邻接、缓存世界矩阵 + 脏标记;`ComponentStorage<T>`(sparse-set)隐藏在接口后,经 Scene 模板 API 增删查。`RenderSystem` 产出**纯数据** `RenderView`(纹理用 RHI 无关的 `uint32_t textureId` 引用),`RenderView→SpriteBatch` 的桥接放在 **sandbox**(运行时/应用层,在 Engine 模块出现前代行其职),从而保持 `me_scene` 与渲染解耦。Demo 用**单一图集**:SpriteBatch 的稳定纹理排序天然保留 Y 序 → 正确 2.5D 叠压 + 1 drawcall。

**Tech Stack:** C++17、CMake(STATIC 库 + 单向依赖)、doctest(WSL 跨平台单测)、DirectX 12(仅 sandbox 目视,M4 不新增 GPU 渲染代码)。

## Global Constraints

(每个任务的要求都隐含包含本节,数值逐字取自 spec / CLAUDE.md。)

- C++ 标准:`CMAKE_CXX_STANDARD 17`,`CMAKE_CXX_STANDARD_REQUIRED ON`,`CMAKE_CXX_EXTENSIONS OFF`。
- **不使用 C++ 异常**:可恢复错误用返回值 / `std::optional`;不变量违反用 `ME_ASSERT` / `ME_ASSERT_MSG`。
- **无硬编码 / 零魔法数字**:数值用具名 `constexpr` 或来自数据。如 `constexpr float kDefaultPivot = 0.5f;`。
- **有注释**:每个公开 API 写 Doxygen 注释;非显然实现写行内注释。
- 头文件中禁止 `using namespace std`。**禁止 Singleton / 全局可变状态**;引擎状态显式传参(System 接收 `Scene&`)。
- **组合优于继承**:Component 是纯数据 + 极少逻辑,更新逻辑在 System;组件存储隐藏在 `ComponentStorage` 接口后(保留向纯 ECS 演进的路径)。
- 模块依赖严格单向:`me_scene → me_core, me_assets`。`me_scene` **不得**依赖 `me_rhi` / `me_renderer`;`RenderView` 用 RHI 无关的 `uint32_t textureId` 引用纹理。
- MSVC 必须 `/utf-8`(源文件含中文注释)。Windows 定义 `UNICODE/_UNICODE/NOMINMAX`(由根 CMake 提供)。
- 新增模块/文件须同步更新对应 `CMakeLists.txt` 与模块 `README.md`。
- 坐标 / 矩阵约定:世界空间 **Y 轴向上**、像素单位;`Matrix4x4` **行主序存储 + 行向量**(`p' = p * M`),平移在第 4 行;**子→世界 = 局部矩阵 * 父世界矩阵**(`world = local * parentWorld`)。
- `me::Rect` = 左下角 (x,y) + 尺寸;`SpriteDesc`/`RenderItem` 的 `srcRect` 为归一化 UV、`dstRect` 为世界像素矩形。

---

## File Structure

**新建(`me_scene`,跨平台,始终构建):**
- `engine/scene/include/me/scene/Entity.h` — `Entity` 句柄类型别名。
- `engine/scene/include/me/scene/Scene.h` + `engine/scene/src/Scene.cpp` — `Scene`:实体生命周期 + 层级 + 世界矩阵解析 + 组件 API(模板部分在头文件)。
- `engine/scene/include/me/scene/ComponentStorage.h` — `IComponentStorage` 接口 + `ComponentStorage<T>` 模板(header-only)。
- `engine/scene/include/me/scene/Components.h` — 数据组件:`SpriteComponent` / `CameraComponent` / `TileMapComponent`。
- `engine/scene/include/me/scene/RenderView.h` — 纯数据 `RenderItem` / `RenderView`。
- `engine/scene/include/me/scene/TransformSystem.h` + `engine/scene/src/TransformSystem.cpp` — 世界矩阵批解析。
- `engine/scene/include/me/scene/RenderSystem.h` + `engine/scene/src/RenderSystem.cpp` — 收集 Sprite → 排序 → `RenderView`;解析活动相机 → `CameraView`。
- `engine/scene/include/me/scene/CameraView.h` — 纯数据相机参数。
- `engine/scene/CMakeLists.txt`、`engine/scene/README.md`。

**新建(测试,WSL doctest):**
- `tests/scene/test_entity.cpp`、`tests/scene/test_transform_hierarchy.cpp`、`tests/scene/test_component_storage.cpp`、`tests/scene/test_render_system.cpp`、`tests/scene/test_camera_system.cpp`。

**修改:**
- `CMakeLists.txt`(加 `add_subdirectory(engine/scene)`,在 assets 之后、renderer 之前)。
- `tests/CMakeLists.txt`(注册 scene 测试源 + 链接 `me_scene`)。
- `sandbox/main.cpp`(用 Scene + System 驱动:瓦片地图地面 + 多个 Sprite 实体经 `RenderView` 桥接渲染,Y 排序叠压)。
- `engine/renderer/README.md`(补充 `RenderView` 桥接约定一小节)。
- `docs/PROGRESS.md`、`docs/architecture/`(新增 M4 ADR)。

---

## Task 1: 实体生命周期(Scene 创建/销毁/有效性 + 槽位回收)+ 模块骨架

**Files:**
- Create: `engine/scene/include/me/scene/Entity.h`
- Create: `engine/scene/include/me/scene/Scene.h`
- Create: `engine/scene/src/Scene.cpp`
- Create: `engine/scene/CMakeLists.txt`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`
- Test: `tests/scene/test_entity.cpp`

**Interfaces:**
- Consumes: `me::Handle<T>`(Core)、`me::Transform2D`、`me::Matrix4x4`(后续任务用)。
- Produces:
  - `namespace me::scene { struct EntityTag; using Entity = me::Handle<EntityTag>; }`
  - `class me::scene::Scene`,本任务方法:
    - `Entity CreateEntity();`
    - `void DestroyEntity(Entity e);` — 连同其子树(后续任务的子节点)与组件一并销毁。
    - `bool IsAlive(Entity e) const;`
    - `std::size_t AliveCount() const;`
    - `std::vector<Entity> AliveEntities() const;`

- [ ] **Step 1: 写实体句柄头文件**

创建 `engine/scene/include/me/scene/Entity.h`:

```cpp
#pragma once

#include "me/core/Handle.h"

namespace me::scene {

/// 仅作类型标签,使 Entity 与其它 Handle<T> 不可互相赋值(编译期类型安全)。
struct EntityTag;

/// 场景实体句柄:index + generation,销毁后旧句柄因 generation 不匹配而失效。
using Entity = me::Handle<EntityTag>;

} // namespace me::scene
```

- [ ] **Step 2: 写 Scene 头文件(本任务部分)**

创建 `engine/scene/include/me/scene/Scene.h`:

```cpp
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

    std::vector<Slot> m_slots;
    std::vector<std::uint32_t> m_freeList; // 可复用槽位 index
    std::size_t m_aliveCount = 0;

    // —— 后续任务在此追加:层级、世界矩阵解析、组件存储 ——
};

} // namespace me::scene
```

- [ ] **Step 3: 写失败测试**

创建 `tests/scene/test_entity.cpp`:

```cpp
#include <doctest/doctest.h>

#include "me/scene/Scene.h"

using me::scene::Entity;
using me::scene::Scene;

TEST_CASE("Scene:新建实体存活、计数递增") {
    Scene scene;
    CHECK(scene.AliveCount() == 0);
    const Entity a = scene.CreateEntity();
    const Entity b = scene.CreateEntity();
    CHECK(a.IsValid());
    CHECK(a != b);
    CHECK(scene.IsAlive(a));
    CHECK(scene.IsAlive(b));
    CHECK(scene.AliveCount() == 2);
}

TEST_CASE("Scene:销毁后句柄失效、计数递减") {
    Scene scene;
    const Entity a = scene.CreateEntity();
    scene.DestroyEntity(a);
    CHECK_FALSE(scene.IsAlive(a));
    CHECK(scene.AliveCount() == 0);
}

TEST_CASE("Scene:槽位复用——旧句柄因 generation 不匹配而失效") {
    Scene scene;
    const Entity a = scene.CreateEntity();
    scene.DestroyEntity(a);
    const Entity b = scene.CreateEntity(); // 复用同一槽位
    CHECK(b.index == a.index);             // 同槽位
    CHECK(b.generation != a.generation);   // 代号已递增
    CHECK_FALSE(scene.IsAlive(a));         // 旧句柄悬垂
    CHECK(scene.IsAlive(b));
}

TEST_CASE("Scene:AliveEntities 只含存活实体") {
    Scene scene;
    const Entity a = scene.CreateEntity();
    const Entity b = scene.CreateEntity();
    scene.DestroyEntity(a);
    const auto alive = scene.AliveEntities();
    REQUIRE(alive.size() == 1);
    CHECK(alive[0] == b);
}

TEST_CASE("Scene:对失效句柄 DestroyEntity 安全无副作用") {
    Scene scene;
    const Entity a = scene.CreateEntity();
    scene.DestroyEntity(a);
    scene.DestroyEntity(a); // 重复销毁不崩溃、不改变计数
    CHECK(scene.AliveCount() == 0);
}
```

- [ ] **Step 4: 写模块 CMakeLists**

创建 `engine/scene/CMakeLists.txt`:

```cmake
add_library(me_scene STATIC
    src/Scene.cpp
)
target_include_directories(me_scene PUBLIC include)
target_compile_features(me_scene PUBLIC cxx_std_17)
# 单向依赖:scene → core, assets。严禁依赖 rhi / renderer。
target_link_libraries(me_scene PUBLIC me_core me_assets)
```

- [ ] **Step 5: 接入根构建 + 注册测试**

修改 `CMakeLists.txt`,在 `add_subdirectory(engine/assets)` 之后、`add_subdirectory(engine/renderer)` 之前加入:

```cmake
add_subdirectory(engine/scene)
```

修改 `tests/CMakeLists.txt`:在 `render/test_camera.cpp` 之后加入测试源:

```cmake
    scene/test_entity.cpp
```

并在 `target_link_libraries(me_tests PRIVATE ...)` 行追加 `me_scene`:

```cmake
target_link_libraries(me_tests PRIVATE doctest::doctest me_core me_platform me_rhi_cpu me_assets me_scene)
```

- [ ] **Step 6: 运行测试,确认失败**

Run:
```bash
cmake -S . -B build -DME_BUILD_TESTS=ON && cmake --build build --target me_tests
```
Expected: 链接/编译失败 —— 找不到 `Scene::CreateEntity` 等定义(`Scene.cpp` 尚未实现)。

- [ ] **Step 7: 写 Scene.cpp 最小实现**

创建 `engine/scene/src/Scene.cpp`:

```cpp
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

} // namespace me::scene
```

> 说明:`RemoveAllComponents(e)` 在本任务先作为**空私有方法**声明实现,Task 3 再填充真实逻辑(这样 Task 1 可独立编译通过)。在 `Scene.h` 的 private 段(`SlotOf` 声明附近)加入:
> ```cpp
>     // 销毁实体时移除其全部组件(Task 3 填充存储遍历)。
>     void RemoveAllComponents(Entity e);
> ```
> 并在 `Scene.cpp` 末尾(`me::scene` 命名空间内)加入空实现:
> ```cpp
> void Scene::RemoveAllComponents(Entity) { /* Task 3 填充 */ }
> ```

- [ ] **Step 8: 运行测试,确认通过**

Run:
```bash
cmake --build build --target me_tests && ./build/bin/me_tests -tc="Scene:*"
```
Expected: PASS(5 个用例)。再跑全量 `./build/bin/me_tests` 确认无回归。

- [ ] **Step 9: 提交**

```bash
git add engine/scene/include/me/scene/Entity.h \
        engine/scene/include/me/scene/Scene.h engine/scene/src/Scene.cpp \
        engine/scene/CMakeLists.txt CMakeLists.txt \
        tests/scene/test_entity.cpp tests/CMakeLists.txt
git commit -m "feat(scene): M4 Entity/Scene 生命周期 + 槽位回收(me_scene 骨架)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 2: Transform 父子层级 + TransformSystem(局部→世界 + 脏标记)

**Files:**
- Modify: `engine/scene/include/me/scene/Scene.h`
- Modify: `engine/scene/src/Scene.cpp`
- Create: `engine/scene/include/me/scene/TransformSystem.h`
- Create: `engine/scene/src/TransformSystem.cpp`
- Modify: `engine/scene/CMakeLists.txt`
- Test: `tests/scene/test_transform_hierarchy.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `Scene`(Task 1)、`me::Transform2D::ToMatrix()`、`me::Matrix4x4::operator*` / `TransformPoint`。
- Produces:
  - `Scene` 新增:
    - `void SetLocalTransform(Entity e, const me::Transform2D& t);`
    - `const me::Transform2D& LocalTransform(Entity e) const;`
    - `void SetParent(Entity child, Entity parent);`(`parent` 传 `Entity::Invalid()` 表示脱离)
    - `Entity Parent(Entity e) const;`
    - `const std::vector<Entity>& Children(Entity e) const;`
    - `const me::Matrix4x4& WorldMatrix(Entity e);`(惰性解析 + 缓存,父先于子)
    - `bool IsWorldDirty(Entity e) const;`
  - `class me::scene::TransformSystem { static void UpdateWorldTransforms(Scene& scene); };`

- [ ] **Step 1: 写失败测试**

创建 `tests/scene/test_transform_hierarchy.cpp`:

```cpp
#include <doctest/doctest.h>

#include "me/scene/Scene.h"
#include "me/scene/TransformSystem.h"

using me::scene::Entity;
using me::scene::Scene;
using me::scene::TransformSystem;

namespace {
constexpr float kEps = 1e-4f;

me::Transform2D At(float x, float y) {
    me::Transform2D t;
    t.position = me::Vector2{x, y};
    return t;
}
} // namespace

TEST_CASE("Transform:无父实体世界平移=局部平移") {
    Scene scene;
    const Entity e = scene.CreateEntity();
    scene.SetLocalTransform(e, At(10.0f, 5.0f));
    TransformSystem::UpdateWorldTransforms(scene);
    const me::Vector2 p = scene.WorldMatrix(e).TransformPoint(me::Vector2{0.0f, 0.0f});
    CHECK(p.x == doctest::Approx(10.0f).epsilon(kEps));
    CHECK(p.y == doctest::Approx(5.0f).epsilon(kEps));
}

TEST_CASE("Transform:子世界=局部叠加父平移") {
    Scene scene;
    const Entity parent = scene.CreateEntity();
    const Entity child = scene.CreateEntity();
    scene.SetLocalTransform(parent, At(10.0f, 0.0f));
    scene.SetLocalTransform(child, At(5.0f, 0.0f));
    scene.SetParent(child, parent);
    TransformSystem::UpdateWorldTransforms(scene);
    const me::Vector2 p = scene.WorldMatrix(child).TransformPoint(me::Vector2{0.0f, 0.0f});
    CHECK(p.x == doctest::Approx(15.0f).epsilon(kEps)); // 5 + 10
    CHECK(p.y == doctest::Approx(0.0f).epsilon(kEps));
}

TEST_CASE("Transform:父缩放作用于子(world = local * parentWorld)") {
    Scene scene;
    const Entity parent = scene.CreateEntity();
    const Entity child = scene.CreateEntity();
    me::Transform2D pt = At(10.0f, 0.0f);
    pt.scale = me::Vector2{2.0f, 2.0f};
    scene.SetLocalTransform(parent, pt);
    scene.SetLocalTransform(child, At(5.0f, 0.0f));
    scene.SetParent(child, parent);
    TransformSystem::UpdateWorldTransforms(scene);
    // child 原点 → 局部(5,0) 经父缩放2+平移10 → (5*2+10, 0) = (20,0)
    const me::Vector2 p = scene.WorldMatrix(child).TransformPoint(me::Vector2{0.0f, 0.0f});
    CHECK(p.x == doctest::Approx(20.0f).epsilon(kEps));
}

TEST_CASE("Transform:移动父→子脏标记重算") {
    Scene scene;
    const Entity parent = scene.CreateEntity();
    const Entity child = scene.CreateEntity();
    scene.SetLocalTransform(parent, At(10.0f, 0.0f));
    scene.SetLocalTransform(child, At(5.0f, 0.0f));
    scene.SetParent(child, parent);
    TransformSystem::UpdateWorldTransforms(scene);
    // 移动父:子的世界缓存应被标记脏并在下次更新重算。
    scene.SetLocalTransform(parent, At(100.0f, 0.0f));
    CHECK(scene.IsWorldDirty(child));
    TransformSystem::UpdateWorldTransforms(scene);
    const me::Vector2 p = scene.WorldMatrix(child).TransformPoint(me::Vector2{0.0f, 0.0f});
    CHECK(p.x == doctest::Approx(105.0f).epsilon(kEps));
}

TEST_CASE("Transform:SetParent 维护 children 邻接") {
    Scene scene;
    const Entity parent = scene.CreateEntity();
    const Entity child = scene.CreateEntity();
    scene.SetParent(child, parent);
    REQUIRE(scene.Children(parent).size() == 1);
    CHECK(scene.Children(parent)[0] == child);
    CHECK(scene.Parent(child) == parent);
    // 脱离父
    scene.SetParent(child, Entity::Invalid());
    CHECK(scene.Children(parent).empty());
    CHECK_FALSE(scene.Parent(child).IsValid());
}
```

- [ ] **Step 2: 注册测试 + TransformSystem.cpp 源**

修改 `tests/CMakeLists.txt`,在 `scene/test_entity.cpp` 后加入:

```cmake
    scene/test_transform_hierarchy.cpp
```

修改 `engine/scene/CMakeLists.txt`,把 `src/TransformSystem.cpp` 加入 `add_library(me_scene STATIC ...)`:

```cmake
add_library(me_scene STATIC
    src/Scene.cpp
    src/TransformSystem.cpp
)
```

- [ ] **Step 3: 运行,确认失败**

Run: `cmake -S . -B build -DME_BUILD_TESTS=ON && cmake --build build --target me_tests`
Expected: 编译失败 —— 找不到 `me/scene/TransformSystem.h` / `Scene::SetLocalTransform` 等。

- [ ] **Step 4: 在 Scene.h 追加层级 + 世界矩阵 API**

修改 `engine/scene/include/me/scene/Scene.h`,在 `AliveEntities()` 声明之后、`private:` 之前加入:

```cpp
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
```

并在 `private:` 段(`RemoveAllComponents` 声明附近)加入辅助声明:

```cpp
    // 把以 e 为根的子树全部标记为世界脏(局部/父变更后调用)。
    void MarkSubtreeDirty(Entity e);
    // e 是否为 maybeAncestor 的(传递)后代;用于 SetParent 环路防护。
    bool IsDescendantOf(Entity e, Entity maybeAncestor) const;
```

> 注:`Scene.h` 顶部已 `#include "me/core/Transform2D.h"` 与 `"me/core/Matrix4x4.h"`(Task 1 已含),无需新增 include。

- [ ] **Step 5: 在 Scene.cpp 实现层级 + 世界矩阵**

在 `engine/scene/src/Scene.cpp` 的 `me::scene` 命名空间内(`AliveEntities` 之后)追加:

```cpp
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
    // 从 e 沿父链上行,若遇到 maybeAncestor 则 e 是其后代。
    Entity cur = e;
    while (cur.IsValid()) {
        const Slot* s = SlotOf(cur);
        if (s == nullptr) break;
        if (cur == maybeAncestor) return true;
        cur = s->parent;
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
```

> 实现注意:`WorldMatrix` 递归解析父时不会修改 `m_slots` 大小,但为稳健起见上面重取了 `s`。`ME_ASSERT_MSG` 来自 `me/core/Assert.h` —— 在 `Scene.cpp` 顶部确保 `#include "me/core/Assert.h"`。

修改 `engine/scene/src/Scene.cpp` 顶部 include 段,加入:

```cpp
#include "me/core/Assert.h"
```

- [ ] **Step 6: 写 TransformSystem**

创建 `engine/scene/include/me/scene/TransformSystem.h`:

```cpp
#pragma once

namespace me::scene {

class Scene;

/**
 * @brief 变换系统:批量解析全部存活实体的世界矩阵并清除脏标记。
 *
 * 无状态(静态方法),显式接收 Scene&(禁全局状态)。内部依赖 Scene::WorldMatrix
 * 的惰性父先于子解析,因此对任意遍历顺序都正确。
 */
class TransformSystem {
public:
    /// @brief 解析所有脏实体的世界矩阵(幂等:已是干净的实体零开销)。
    static void UpdateWorldTransforms(Scene& scene);
};

} // namespace me::scene
```

创建 `engine/scene/src/TransformSystem.cpp`:

```cpp
#include "me/scene/TransformSystem.h"

#include "me/scene/Scene.h"

namespace me::scene {

void TransformSystem::UpdateWorldTransforms(Scene& scene) {
    // WorldMatrix 惰性递归解析父链并缓存;遍历全部存活实体即可全部解析。
    for (const Entity e : scene.AliveEntities()) {
        scene.WorldMatrix(e);
    }
}

} // namespace me::scene
```

- [ ] **Step 7: 运行,确认通过**

Run: `cmake --build build --target me_tests && ./build/bin/me_tests -tc="Transform:*"`
Expected: PASS(5 个用例)。再跑全量 `./build/bin/me_tests` 确认无回归。

- [ ] **Step 8: 提交**

```bash
git add engine/scene/include/me/scene/Scene.h engine/scene/src/Scene.cpp \
        engine/scene/include/me/scene/TransformSystem.h \
        engine/scene/src/TransformSystem.cpp engine/scene/CMakeLists.txt \
        tests/scene/test_transform_hierarchy.cpp tests/CMakeLists.txt
git commit -m "feat(scene): M4 Transform 父子层级 + TransformSystem(脏标记/世界矩阵)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 3: ComponentStorage<T> + Scene 组件 API(增删查 + 销毁清理)

**Files:**
- Create: `engine/scene/include/me/scene/ComponentStorage.h`
- Modify: `engine/scene/include/me/scene/Scene.h`
- Modify: `engine/scene/src/Scene.cpp`
- Test: `tests/scene/test_component_storage.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `Scene`、`Entity`(Task 1)。
- Produces:
  - `class me::scene::IComponentStorage { virtual ~...; virtual void Remove(Entity)=0; virtual bool Has(Entity) const=0; };`
  - `template<class T> class me::scene::ComponentStorage : public IComponentStorage` —— `Add/Get/Has/Remove`,以及 `Entities()`(`const std::vector<Entity>&`)与 `Items()`(`std::vector<T>&`)并行数组供 System 遍历。
  - `Scene` 新增模板 API:
    - `template<class T> T& AddComponent(Entity e, const T& value = T{});`
    - `template<class T> T* GetComponent(Entity e);`
    - `template<class T> bool HasComponent(Entity e) const;`
    - `template<class T> void RemoveComponent(Entity e);`
    - `template<class T> ComponentStorage<T>& ComponentStore();`(创建即用,供 System 遍历)
  - `Scene::RemoveAllComponents(Entity)` 真实实现(销毁实体时遍历全部存储移除)。

- [ ] **Step 1: 写 ComponentStorage 头文件**

创建 `engine/scene/include/me/scene/ComponentStorage.h`:

```cpp
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
```

- [ ] **Step 2: 写失败测试**

创建 `tests/scene/test_component_storage.cpp`:

```cpp
#include <doctest/doctest.h>

#include "me/scene/Scene.h"

using me::scene::Entity;
using me::scene::Scene;

namespace {
struct Health { int value = 0; };
struct Tag { };
} // namespace

TEST_CASE("Components:增/查/取") {
    Scene scene;
    const Entity e = scene.CreateEntity();
    CHECK_FALSE(scene.HasComponent<Health>(e));
    scene.AddComponent<Health>(e, Health{42});
    CHECK(scene.HasComponent<Health>(e));
    REQUIRE(scene.GetComponent<Health>(e) != nullptr);
    CHECK(scene.GetComponent<Health>(e)->value == 42);
}

TEST_CASE("Components:多类型互不干扰") {
    Scene scene;
    const Entity e = scene.CreateEntity();
    scene.AddComponent<Health>(e, Health{1});
    scene.AddComponent<Tag>(e, Tag{});
    CHECK(scene.HasComponent<Health>(e));
    CHECK(scene.HasComponent<Tag>(e));
    CHECK_FALSE(scene.HasComponent<Tag>(scene.CreateEntity()));
}

TEST_CASE("Components:移除") {
    Scene scene;
    const Entity e = scene.CreateEntity();
    scene.AddComponent<Health>(e, Health{7});
    scene.RemoveComponent<Health>(e);
    CHECK_FALSE(scene.HasComponent<Health>(e));
    CHECK(scene.GetComponent<Health>(e) == nullptr);
}

TEST_CASE("Components:销毁实体自动清理其组件") {
    Scene scene;
    const Entity a = scene.CreateEntity();
    const Entity b = scene.CreateEntity();
    scene.AddComponent<Health>(a, Health{10});
    scene.AddComponent<Health>(b, Health{20});
    scene.DestroyEntity(a);
    // a 的组件已随实体销毁;b 不受影响。
    CHECK(scene.ComponentStore<Health>().Size() == 1);
    REQUIRE(scene.GetComponent<Health>(b) != nullptr);
    CHECK(scene.GetComponent<Health>(b)->value == 20);
}

TEST_CASE("Components:槽位复用后新实体不继承旧组件") {
    Scene scene;
    const Entity a = scene.CreateEntity();
    scene.AddComponent<Health>(a, Health{99});
    scene.DestroyEntity(a);
    const Entity b = scene.CreateEntity(); // 复用槽位
    CHECK_FALSE(scene.HasComponent<Health>(b));
}
```

- [ ] **Step 3: 注册测试**

修改 `tests/CMakeLists.txt`,在 `scene/test_transform_hierarchy.cpp` 后加入:

```cmake
    scene/test_component_storage.cpp
```

- [ ] **Step 4: 运行,确认失败**

Run: `cmake --build build --target me_tests`
Expected: 编译失败 —— `Scene::AddComponent` 等未声明。

- [ ] **Step 5: 在 Scene.h 加组件存储 + 模板 API**

修改 `engine/scene/include/me/scene/Scene.h`:

顶部 include 段加入:

```cpp
#include <memory>
#include <typeindex>
#include <unordered_map>

#include "me/scene/ComponentStorage.h"
```

在 `WorldMatrix` / `IsWorldDirty` 之后(仍在 `public:` 段)加入模板 API:

```cpp
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
```

在 `Scene.h` 的 `private:` 段加入存储容器与查找辅助(`ASSERT` 已随 ComponentStorage.h 间接可用,但 Scene.h 用到 `ME_ASSERT_MSG`,确保已 `#include "me/core/Assert.h"`):

```cpp
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
```

> 注:`Scene.h` 在 Task 1 已 `#include "me/core/Assert.h"` 吗?Task 1 未包含。本步在顶部 include 段补上:
> ```cpp
> #include "me/core/Assert.h"
> ```

- [ ] **Step 6: 在 Scene.cpp 实现 RemoveAllComponents**

修改 `engine/scene/src/Scene.cpp`,把 Task 1 的空实现替换为遍历全部存储:

```cpp
void Scene::RemoveAllComponents(Entity e) {
    for (auto& kv : m_stores) kv.second->Remove(e);
}
```

- [ ] **Step 7: 运行,确认通过**

Run: `cmake --build build --target me_tests && ./build/bin/me_tests -tc="Components:*"`
Expected: PASS(5 个用例)。再跑全量确认无回归。

- [ ] **Step 8: 提交**

```bash
git add engine/scene/include/me/scene/ComponentStorage.h \
        engine/scene/include/me/scene/Scene.h engine/scene/src/Scene.cpp \
        tests/scene/test_component_storage.cpp tests/CMakeLists.txt
git commit -m "feat(scene): M4 ComponentStorage<T> + Scene 组件增删查(销毁自动清理)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 4: SpriteComponent + RenderView + RenderSystem(世界 dstRect + 层/Y 排序)

**Files:**
- Create: `engine/scene/include/me/scene/Components.h`
- Create: `engine/scene/include/me/scene/RenderView.h`
- Create: `engine/scene/include/me/scene/RenderSystem.h`
- Create: `engine/scene/src/RenderSystem.cpp`
- Modify: `engine/scene/CMakeLists.txt`
- Test: `tests/scene/test_render_system.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `Scene`、`Scene::WorldMatrix`、`Scene::ComponentStore<SpriteComponent>`、`me::Rect`/`me::Vector2`/`me::Vector4`/`me::Matrix4x4`。
- Produces:
  - `struct me::scene::SpriteComponent { std::uint32_t textureId; me::Rect srcRect; me::Vector2 size; me::Vector2 pivot; me::Vector4 color; int sortLayer; };`
  - `struct me::scene::RenderItem { std::uint32_t textureId; me::Rect srcRect; me::Rect dstRect; me::Vector4 color; float rotation; int sortLayer; };`
  - `using me::scene::RenderView = std::vector<RenderItem>;`
  - `class me::scene::RenderSystem { static RenderView BuildRenderView(Scene& scene); };`(按 `sortLayer` 升序、再按世界 Y 降序稳定排序)

- [ ] **Step 1: 写组件 + RenderView 头文件**

创建 `engine/scene/include/me/scene/Components.h`:

```cpp
#pragma once

#include <cstdint>

#include "me/core/Rect.h"
#include "me/core/Vector2.h"
#include "me/core/Vector4.h"

namespace me::scene {

/// 精灵锚点默认值:0.5 = 中心(与 SpriteDesc 绕中心旋转一致)。
constexpr float kDefaultPivot = 0.5f;

/// 无纹理引用哨兵(textureId 的无效值)。
constexpr std::uint32_t kInvalidTextureId = 0xFFFFFFFFu;

/**
 * @brief 精灵渲染组件(纯数据)。纹理用 RHI 无关的 textureId 引用,
 *        在渲染边界(sandbox/Engine)解析为实际 GpuTexture*,保持 Scene 与 RHI 解耦。
 *
 * size:世界像素尺寸;pivot:归一化锚点(0..1),决定 size 相对实体世界位置的摆放;
 * sortLayer:主排序键(小在底);同层内按世界 Y 降序(高 Y 在后,低 Y 压在上,2.5D)。
 */
struct SpriteComponent {
    std::uint32_t textureId = kInvalidTextureId;
    me::Rect srcRect{0.0f, 0.0f, 1.0f, 1.0f};
    me::Vector2 size{0.0f, 0.0f};
    me::Vector2 pivot{kDefaultPivot, kDefaultPivot};
    me::Vector4 color{1.0f, 1.0f, 1.0f, 1.0f};
    int sortLayer = 0;
};

} // namespace me::scene
```

创建 `engine/scene/include/me/scene/RenderView.h`:

```cpp
#pragma once

#include <cstdint>
#include <vector>

#include "me/core/Rect.h"
#include "me/core/Vector4.h"

namespace me::scene {

/**
 * @brief 一条渲染指令(纯数据,RHI 无关)。RenderSystem 产出,渲染边界消费:
 *        把 textureId 解析为 GpuTexture* 后填入 SpriteDesc 提交给 SpriteBatch。
 *
 * dstRect:世界像素矩形(左下原点、Y 向上);srcRect:归一化 UV;rotation:绕中心弧度。
 */
struct RenderItem {
    std::uint32_t textureId = 0;
    me::Rect srcRect{0.0f, 0.0f, 1.0f, 1.0f};
    me::Rect dstRect{};
    me::Vector4 color{1.0f, 1.0f, 1.0f, 1.0f};
    float rotation = 0.0f;
    int sortLayer = 0;
};

/// 一帧的可见渲染指令序列,已按(层升序、世界 Y 降序)稳定排序。
using RenderView = std::vector<RenderItem>;

} // namespace me::scene
```

- [ ] **Step 2: 写失败测试**

创建 `tests/scene/test_render_system.cpp`:

```cpp
#include <doctest/doctest.h>

#include "me/scene/Scene.h"
#include "me/scene/Components.h"
#include "me/scene/RenderSystem.h"
#include "me/scene/TransformSystem.h"

using me::scene::Entity;
using me::scene::Scene;
using me::scene::SpriteComponent;
using me::scene::RenderSystem;
using me::scene::RenderView;
using me::scene::TransformSystem;

namespace {
constexpr float kEps = 1e-4f;

// 在世界 (x,y) 放一个 size×size、锚点中心、指定层的精灵实体。
Entity MakeSprite(Scene& s, float x, float y, float size, int layer,
                  std::uint32_t tex = 0) {
    const Entity e = s.CreateEntity();
    me::Transform2D t;
    t.position = me::Vector2{x, y};
    s.SetLocalTransform(e, t);
    SpriteComponent sp;
    sp.textureId = tex;
    sp.size = me::Vector2{size, size};
    sp.sortLayer = layer;
    s.AddComponent<SpriteComponent>(e, sp);
    return e;
}
} // namespace

TEST_CASE("RenderSystem:dstRect 由世界位置 + size + 中心锚点导出") {
    Scene scene;
    MakeSprite(scene, 100.0f, 50.0f, 16.0f, 0);
    TransformSystem::UpdateWorldTransforms(scene);
    const RenderView view = RenderSystem::BuildRenderView(scene);
    REQUIRE(view.size() == 1);
    // 锚点中心:dst 左下 = (100 - 8, 50 - 8)。
    CHECK(view[0].dstRect.x == doctest::Approx(92.0f).epsilon(kEps));
    CHECK(view[0].dstRect.y == doctest::Approx(42.0f).epsilon(kEps));
    CHECK(view[0].dstRect.width == doctest::Approx(16.0f).epsilon(kEps));
    CHECK(view[0].dstRect.height == doctest::Approx(16.0f).epsilon(kEps));
}

TEST_CASE("RenderSystem:先按层升序排序") {
    Scene scene;
    MakeSprite(scene, 0.0f, 0.0f, 8.0f, /*layer*/2, /*tex*/20);
    MakeSprite(scene, 0.0f, 0.0f, 8.0f, /*layer*/0, /*tex*/10);
    MakeSprite(scene, 0.0f, 0.0f, 8.0f, /*layer*/1, /*tex*/15);
    TransformSystem::UpdateWorldTransforms(scene);
    const RenderView view = RenderSystem::BuildRenderView(scene);
    REQUIRE(view.size() == 3);
    CHECK(view[0].sortLayer == 0);
    CHECK(view[1].sortLayer == 1);
    CHECK(view[2].sortLayer == 2);
}

TEST_CASE("RenderSystem:同层内按世界 Y 降序(高 Y 在前,低 Y 压在上)") {
    Scene scene;
    MakeSprite(scene, 0.0f, /*y*/10.0f, 8.0f, 0, /*tex*/1);
    MakeSprite(scene, 0.0f, /*y*/90.0f, 8.0f, 0, /*tex*/2);
    MakeSprite(scene, 0.0f, /*y*/50.0f, 8.0f, 0, /*tex*/3);
    TransformSystem::UpdateWorldTransforms(scene);
    const RenderView view = RenderSystem::BuildRenderView(scene);
    REQUIRE(view.size() == 3);
    // Y 降序:90 → 50 → 10(低 Y 最后提交 = 画在最上,符合 2.5D)。
    CHECK(view[0].dstRect.y > view[1].dstRect.y);
    CHECK(view[1].dstRect.y > view[2].dstRect.y);
    CHECK(view[0].textureId == 2);
    CHECK(view[2].textureId == 1);
}

TEST_CASE("RenderSystem:无 SpriteComponent 的实体被忽略") {
    Scene scene;
    scene.CreateEntity(); // 无精灵
    MakeSprite(scene, 0.0f, 0.0f, 8.0f, 0);
    TransformSystem::UpdateWorldTransforms(scene);
    CHECK(RenderSystem::BuildRenderView(scene).size() == 1);
}

TEST_CASE("RenderSystem:srcRect/color/textureId 透传") {
    Scene scene;
    const Entity e = scene.CreateEntity();
    scene.SetLocalTransform(e, me::Transform2D{});
    SpriteComponent sp;
    sp.textureId = 7;
    sp.srcRect = me::Rect{0.25f, 0.5f, 0.25f, 0.25f};
    sp.color = me::Vector4{0.1f, 0.2f, 0.3f, 0.4f};
    sp.size = me::Vector2{8.0f, 8.0f};
    scene.AddComponent<SpriteComponent>(e, sp);
    TransformSystem::UpdateWorldTransforms(scene);
    const RenderView view = RenderSystem::BuildRenderView(scene);
    REQUIRE(view.size() == 1);
    CHECK(view[0].textureId == 7);
    CHECK(view[0].srcRect.x == doctest::Approx(0.25f).epsilon(kEps));
    CHECK(view[0].color.y == doctest::Approx(0.2f).epsilon(kEps));
}
```

- [ ] **Step 3: 注册测试 + RenderSystem.cpp 源**

修改 `tests/CMakeLists.txt`,在 `scene/test_component_storage.cpp` 后加入:

```cmake
    scene/test_render_system.cpp
```

修改 `engine/scene/CMakeLists.txt`,把 `src/RenderSystem.cpp` 加入源列表:

```cmake
add_library(me_scene STATIC
    src/Scene.cpp
    src/TransformSystem.cpp
    src/RenderSystem.cpp
)
```

- [ ] **Step 4: 运行,确认失败**

Run: `cmake --build build --target me_tests`
Expected: 编译失败 —— 找不到 `me/scene/RenderSystem.h`。

- [ ] **Step 5: 写 RenderSystem 头文件**

创建 `engine/scene/include/me/scene/RenderSystem.h`:

```cpp
#pragma once

#include "me/scene/RenderView.h"

namespace me::scene {

class Scene;

/**
 * @brief 渲染系统:收集存活实体的 SpriteComponent,结合世界变换算 dstRect,
 *        产出按(层升序、世界 Y 降序)稳定排序的 RenderView。
 *
 * 纯数据输出,不碰 RHI/Renderer。调用前应先 TransformSystem::UpdateWorldTransforms。
 */
class RenderSystem {
public:
    static RenderView BuildRenderView(Scene& scene);
};

} // namespace me::scene
```

- [ ] **Step 6: 写 RenderSystem 实现**

创建 `engine/scene/src/RenderSystem.cpp`:

```cpp
#include "me/scene/RenderSystem.h"

#include <algorithm>
#include <cmath>

#include "me/scene/Scene.h"
#include "me/scene/Components.h"
#include "me/core/Matrix4x4.h"
#include "me/core/Vector2.h"

namespace me::scene {

namespace {

// 从 2D 世界矩阵分解出平移、旋转(弧度)、缩放(用基向量长度/方向)。
struct World2D {
    me::Vector2 position;
    float rotation;
    me::Vector2 scale;
};

World2D DecomposeWorld(const me::Matrix4x4& m) {
    World2D w;
    w.position = m.TransformPoint(me::Vector2{0.0f, 0.0f});
    const me::Vector2 right = m.TransformVector(me::Vector2{1.0f, 0.0f});
    const me::Vector2 up = m.TransformVector(me::Vector2{0.0f, 1.0f});
    w.scale = me::Vector2{right.Length(), up.Length()};
    w.rotation = std::atan2(right.y, right.x);
    return w;
}

} // namespace

RenderView RenderSystem::BuildRenderView(Scene& scene) {
    ComponentStorage<SpriteComponent>& store = scene.ComponentStore<SpriteComponent>();
    const std::vector<Entity>& owners = store.Entities();
    const std::vector<SpriteComponent>& sprites = store.Items();

    RenderView view;
    view.reserve(sprites.size());
    for (std::size_t i = 0; i < sprites.size(); ++i) {
        const Entity e = owners[i];
        if (!scene.IsAlive(e)) continue; // 防御:存储应已随销毁清理
        const SpriteComponent& sp = sprites[i];
        const World2D w = DecomposeWorld(scene.WorldMatrix(e));

        RenderItem item;
        item.textureId = sp.textureId;
        item.srcRect = sp.srcRect;
        item.color = sp.color;
        item.rotation = w.rotation;
        item.sortLayer = sp.sortLayer;

        // 世界像素尺寸 = 精灵 size × 世界缩放;按锚点摆放到世界位置。
        const float dstW = sp.size.x * w.scale.x;
        const float dstH = sp.size.y * w.scale.y;
        item.dstRect = me::Rect{
            w.position.x - sp.pivot.x * dstW,
            w.position.y - sp.pivot.y * dstH,
            dstW, dstH,
        };
        view.push_back(item);
    }

    // 稳定排序:层升序为主;同层世界 Y 降序(低 Y 后提交 → 画在最上,2.5D 叠压)。
    std::stable_sort(view.begin(), view.end(),
        [](const RenderItem& a, const RenderItem& b) {
            if (a.sortLayer != b.sortLayer) return a.sortLayer < b.sortLayer;
            // dstRect.y 为左下角;以其作为同层 Y 比较键即可(尺寸一致时等价于中心)。
            return a.dstRect.y > b.dstRect.y;
        });
    return view;
}

} // namespace me::scene
```

- [ ] **Step 7: 运行,确认通过**

Run: `cmake --build build --target me_tests && ./build/bin/me_tests -tc="RenderSystem:*"`
Expected: PASS(5 个用例)。再跑全量确认无回归。

- [ ] **Step 8: 提交**

```bash
git add engine/scene/include/me/scene/Components.h \
        engine/scene/include/me/scene/RenderView.h \
        engine/scene/include/me/scene/RenderSystem.h \
        engine/scene/src/RenderSystem.cpp engine/scene/CMakeLists.txt \
        tests/scene/test_render_system.cpp tests/CMakeLists.txt
git commit -m "feat(scene): M4 SpriteComponent + RenderSystem(世界 dstRect + 层/Y 排序)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 5: CameraComponent + CameraView + 活动相机解析

**Files:**
- Modify: `engine/scene/include/me/scene/Components.h`
- Create: `engine/scene/include/me/scene/CameraView.h`
- Modify: `engine/scene/include/me/scene/Scene.h`
- Modify: `engine/scene/src/Scene.cpp`
- Modify: `engine/scene/include/me/scene/RenderSystem.h`
- Modify: `engine/scene/src/RenderSystem.cpp`
- Test: `tests/scene/test_camera_system.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `Scene`、`Scene::WorldMatrix`、`CameraComponent`。
- Produces:
  - `struct me::scene::CameraComponent { float zoom; me::Vector2 viewportSize; };`
  - `struct me::scene::CameraView { me::Vector2 center; float zoom; me::Vector2 viewportSize; };`(纯数据,sandbox 据此构造 OrthographicCamera)
  - `Scene` 新增:`void SetActiveCamera(Entity e);` / `Entity ActiveCamera() const;`
  - `RenderSystem` 新增:`static std::optional<CameraView> ResolveActiveCamera(Scene& scene);`
    (优先用 `ActiveCamera()`;若未设置或失效则取第一个带 `CameraComponent` 的实体;都没有则 `nullopt`)

- [ ] **Step 1: 写失败测试**

创建 `tests/scene/test_camera_system.cpp`:

```cpp
#include <doctest/doctest.h>

#include "me/scene/Scene.h"
#include "me/scene/Components.h"
#include "me/scene/RenderSystem.h"
#include "me/scene/TransformSystem.h"

using me::scene::Entity;
using me::scene::Scene;
using me::scene::CameraComponent;
using me::scene::CameraView;
using me::scene::RenderSystem;
using me::scene::TransformSystem;

namespace {
constexpr float kEps = 1e-4f;

Entity MakeCamera(Scene& s, float x, float y, float zoom) {
    const Entity e = s.CreateEntity();
    me::Transform2D t;
    t.position = me::Vector2{x, y};
    s.SetLocalTransform(e, t);
    CameraComponent c;
    c.zoom = zoom;
    c.viewportSize = me::Vector2{320.0f, 180.0f};
    s.AddComponent<CameraComponent>(e, c);
    return e;
}
} // namespace

TEST_CASE("Camera:无相机返回 nullopt") {
    Scene scene;
    CHECK_FALSE(RenderSystem::ResolveActiveCamera(scene).has_value());
}

TEST_CASE("Camera:解析活动相机——中心取实体世界位置") {
    Scene scene;
    const Entity cam = MakeCamera(scene, 64.0f, 32.0f, 2.0f);
    scene.SetActiveCamera(cam);
    TransformSystem::UpdateWorldTransforms(scene);
    const auto view = RenderSystem::ResolveActiveCamera(scene);
    REQUIRE(view.has_value());
    CHECK(view->center.x == doctest::Approx(64.0f).epsilon(kEps));
    CHECK(view->center.y == doctest::Approx(32.0f).epsilon(kEps));
    CHECK(view->zoom == doctest::Approx(2.0f).epsilon(kEps));
    CHECK(view->viewportSize.x == doctest::Approx(320.0f).epsilon(kEps));
}

TEST_CASE("Camera:未显式设置活动相机时取第一个相机") {
    Scene scene;
    MakeCamera(scene, 10.0f, 20.0f, 1.0f);
    TransformSystem::UpdateWorldTransforms(scene);
    const auto view = RenderSystem::ResolveActiveCamera(scene);
    REQUIRE(view.has_value());
    CHECK(view->center.x == doctest::Approx(10.0f).epsilon(kEps));
}

TEST_CASE("Camera:相机跟随父——世界位置含父平移") {
    Scene scene;
    const Entity player = scene.CreateEntity();
    me::Transform2D pt;
    pt.position = me::Vector2{200.0f, 0.0f};
    scene.SetLocalTransform(player, pt);
    const Entity cam = MakeCamera(scene, 0.0f, 0.0f, 1.0f);
    scene.SetParent(cam, player);
    scene.SetActiveCamera(cam);
    TransformSystem::UpdateWorldTransforms(scene);
    const auto view = RenderSystem::ResolveActiveCamera(scene);
    REQUIRE(view.has_value());
    CHECK(view->center.x == doctest::Approx(200.0f).epsilon(kEps));
}
```

- [ ] **Step 2: 注册测试**

修改 `tests/CMakeLists.txt`,在 `scene/test_render_system.cpp` 后加入:

```cmake
    scene/test_camera_system.cpp
```

- [ ] **Step 3: 运行,确认失败**

Run: `cmake --build build --target me_tests`
Expected: 编译失败 —— 找不到 `CameraComponent` / `CameraView` / `ResolveActiveCamera`。

- [ ] **Step 4: 加 CameraComponent + CameraView**

修改 `engine/scene/include/me/scene/Components.h`,在 `SpriteComponent` 之后(命名空间内)加入:

```cpp
/**
 * @brief 正交相机组件(纯数据)。相机中心 = 实体世界位置;zoom>1 放大。
 *        viewportSize 为像素视口尺寸(通常等于渲染目标尺寸)。
 */
struct CameraComponent {
    float zoom = 1.0f;
    me::Vector2 viewportSize{0.0f, 0.0f};
};
```

创建 `engine/scene/include/me/scene/CameraView.h`:

```cpp
#pragma once

#include "me/core/Vector2.h"

namespace me::scene {

/**
 * @brief 解析后的相机参数(纯数据,RHI/Renderer 无关)。
 *        渲染边界据此构造 me::render::OrthographicCamera。
 */
struct CameraView {
    me::Vector2 center{0.0f, 0.0f};
    float zoom = 1.0f;
    me::Vector2 viewportSize{0.0f, 0.0f};
};

} // namespace me::scene
```

- [ ] **Step 5: Scene 加活动相机字段**

修改 `engine/scene/include/me/scene/Scene.h`:在组件模板 API 之后(`public:` 段)加入:

```cpp
    // —— 活动相机 ——
    /// @brief 指定活动相机实体(RenderSystem::ResolveActiveCamera 优先使用)。
    void SetActiveCamera(Entity e) { m_activeCamera = e; }
    /// @brief 当前活动相机实体(可能无效)。
    Entity ActiveCamera() const { return m_activeCamera; }
```

在 `private:` 段加入字段:

```cpp
    Entity m_activeCamera = Entity::Invalid();
```

> `SetActiveCamera`/`ActiveCamera` 为内联,无需改 `Scene.cpp`。

- [ ] **Step 6: RenderSystem 加 ResolveActiveCamera**

修改 `engine/scene/include/me/scene/RenderSystem.h`:顶部加 include,并在类内加声明:

```cpp
#include <optional>

#include "me/scene/RenderView.h"
#include "me/scene/CameraView.h"
```

```cpp
    /// @brief 解析活动相机为纯数据 CameraView;无可用相机返回 nullopt。
    static std::optional<CameraView> ResolveActiveCamera(Scene& scene);
```

修改 `engine/scene/src/RenderSystem.cpp`:顶部加 include,并在命名空间内追加实现:

```cpp
#include "me/scene/Components.h"   // 已含;确保在内
```

```cpp
std::optional<CameraView> RenderSystem::ResolveActiveCamera(Scene& scene) {
    ComponentStorage<CameraComponent>& store = scene.ComponentStore<CameraComponent>();

    // 优先:显式活动相机且仍持有 CameraComponent。
    Entity cam = scene.ActiveCamera();
    const CameraComponent* comp = scene.IsAlive(cam) ? store.Get(cam) : nullptr;

    // 回退:第一个带 CameraComponent 的存活实体。
    if (comp == nullptr) {
        const std::vector<Entity>& owners = store.Entities();
        for (std::size_t i = 0; i < owners.size(); ++i) {
            if (scene.IsAlive(owners[i])) { cam = owners[i]; comp = &store.Items()[i]; break; }
        }
    }
    if (comp == nullptr) return std::nullopt;

    CameraView view;
    view.center = scene.WorldMatrix(cam).TransformPoint(me::Vector2{0.0f, 0.0f});
    view.zoom = comp->zoom;
    view.viewportSize = comp->viewportSize;
    return view;
}
```

- [ ] **Step 7: 运行,确认通过**

Run: `cmake --build build --target me_tests && ./build/bin/me_tests -tc="Camera:*"`
Expected: PASS(4 个用例)。再跑全量确认无回归。

- [ ] **Step 8: 提交**

```bash
git add engine/scene/include/me/scene/Components.h \
        engine/scene/include/me/scene/CameraView.h \
        engine/scene/include/me/scene/Scene.h \
        engine/scene/include/me/scene/RenderSystem.h \
        engine/scene/src/RenderSystem.cpp \
        tests/scene/test_camera_system.cpp tests/CMakeLists.txt
git commit -m "feat(scene): M4 CameraComponent + 活动相机解析(CameraView)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 6: sandbox 集成 + TileMapComponent + 文档 + 进度回写

**Files:**
- Modify: `engine/scene/include/me/scene/Components.h`(加 `TileMapComponent`)
- Modify: `sandbox/main.cpp`
- Modify: `sandbox/CMakeLists.txt`(链接 `me_scene`)
- Create: `engine/scene/README.md`
- Modify: `engine/renderer/README.md`
- Modify: `docs/PROGRESS.md`
- Create: `docs/architecture/0002-m4-scene-components.md`

**Interfaces:**
- Consumes: `me::scene::{Scene, Entity, SpriteComponent, CameraComponent, TileMapComponent, RenderSystem, TransformSystem, RenderView, CameraView}`、`me::render::{SpriteBatch, SpriteDesc, OrthographicCamera, Tileset, TileMapRenderer}`、`me::assets::{LoadTiledMap, LoadImageRGBA8}`、`me::rhi::GpuTexture`。
- Produces:
  - `struct me::scene::TileMapComponent { const me::assets::TileMapData* map; std::uint32_t textureId; };`(非拥有指针;数据驱动地面层)
  - sandbox 内的 `RenderView → SpriteBatch` 桥接(把 `textureId` 解析为 `GpuTexture*`)。

- [ ] **Step 1: 加 TileMapComponent**

修改 `engine/scene/include/me/scene/Components.h`:顶部 include 加入 `#include "me/assets/TileMapData.h"`,并在命名空间内加入:

```cpp
/**
 * @brief 瓦片地图组件(纯数据,非拥有指针)。map 指向已加载的 TileMapData,
 *        textureId 引用其 tileset 纹理;由渲染边界用 TileMapRenderer 绘制。
 */
struct TileMapComponent {
    const me::assets::TileMapData* map = nullptr;
    std::uint32_t textureId = kInvalidTextureId;
};
```

> 因 `Components.h` 现在 include 了 `me/assets/TileMapData.h`,`me_scene` 已链接 `me_assets`(Task 1 CMake),依赖满足。

- [ ] **Step 2: sandbox 链接 me_scene**

修改 `sandbox/CMakeLists.txt`,在 `target_link_libraries(...)` 中加入 `me_scene`(置于 `me_renderer` 旁;sandbox 作为运行时/应用层可同时知晓 scene 与 renderer)。

> 先打开 `sandbox/CMakeLists.txt` 确认当前链接行,把 `me_scene` 追加进 `PRIVATE`/`PUBLIC` 依赖列表(与现有 `me_renderer me_assets` 同段)。

- [ ] **Step 3: 改写 sandbox —— 用 Scene + System 驱动**

修改 `sandbox/main.cpp`。目标:加载瓦片地图作为**地面**(经 `TileMapRenderer`),并在其上放若干 **Sprite 实体**(共享同一 tileset 图集做纹理,取不同 `srcRect` 当作不同物体),用 `Scene` + `TransformSystem` + `RenderSystem` 产出 `RenderView`,经桥接提交给 `SpriteBatch`,演示 Y 排序叠压。相机用一个挂了 `CameraComponent` 的实体,经 `ResolveActiveCamera` → `OrthographicCamera`。

关键改动:

(a) include 段加入:
```cpp
#include "me/scene/Scene.h"
#include "me/scene/Components.h"
#include "me/scene/RenderSystem.h"
#include "me/scene/TransformSystem.h"
#include "me/scene/CameraView.h"
```

(b) 在地图与 tileset 纹理加载之后(沿用 M3 的 `mapData` / `tileTex` / `tileset`),构建场景。纹理表把 `textureId=0` 映射到 tileset 纹理(demo 单图集):
```cpp
    // textureId → GpuTexture* 解析表(demo 仅一张图集:id 0 = tileset 纹理)。
    constexpr std::uint32_t kTilesetTextureId = 0;
    std::vector<const rhi::GpuTexture*> textureTable;
    textureTable.push_back(tileTex.get()); // index 0

    namespace sc = me::scene;
    sc::Scene scene;

    // 地面:瓦片地图实体(数据驱动,经 TileMapRenderer 绘制)。
    const sc::Entity ground = scene.CreateEntity();
    {
        sc::TileMapComponent tm;
        tm.map = &(*mapData);
        tm.textureId = kTilesetTextureId;
        scene.AddComponent<sc::TileMapComponent>(ground, tm);
    }

    // 几个 Sprite 实体:取 tileset 中不同瓦片作为“物体”,放在地图不同位置。
    // srcRect 用 TileLayout 把局部 id 转 UV(与瓦片同图集)。
    auto spriteSrc = [&](int localId) {
        return me::assets::SrcRectForLocalId(mapData->tileset, localId);
    };
    constexpr float kSpriteSize = 16.0f;
    auto addProp = [&](float x, float y, int localId, int layer) {
        const sc::Entity e = scene.CreateEntity();
        me::Transform2D t; t.position = me::Vector2{x, y};
        scene.SetLocalTransform(e, t);
        sc::SpriteComponent sp;
        sp.textureId = kTilesetTextureId;
        sp.srcRect = spriteSrc(localId);
        sp.size = me::Vector2{kSpriteSize, kSpriteSize};
        sp.sortLayer = layer;
        scene.AddComponent<sc::SpriteComponent>(e, sp);
        return e;
    };
    // 同层、不同 Y 的两个物体,演示 2.5D 叠压;另一个在更高层。
    addProp(80.0f, 96.0f, /*localId*/6, /*layer*/1);
    addProp(96.0f, 64.0f, /*localId*/9, /*layer*/1);
    const sc::Entity player = addProp(96.0f, 80.0f, /*localId*/4, /*layer*/1);

    // 相机实体:挂 CameraComponent,作为 player 的子节点(跟随)。
    const sc::Entity cameraEntity = scene.CreateEntity();
    {
        sc::CameraComponent cc;
        cc.zoom = 1.0f;
        cc.viewportSize = me::Vector2{float(windowWidth), float(windowHeight)};
        scene.AddComponent<sc::CameraComponent>(cameraEntity, cc);
        scene.SetParent(cameraEntity, player);
        scene.SetActiveCamera(cameraEntity);
    }
    sc::Entity tileMapGround = ground; // 供渲染循环引用
```

> 注:`windowWidth`/`windowHeight` 以 sandbox 现有窗口尺寸变量为准(打开文件确认实际名称,替换之)。`localId` 取值需在 tileset 范围内(M3 的 64×48 图集为 4 列 3 行,localId 0..11)。

(c) 渲染循环内:输入(WASD)改为移动 **player** 局部变换(相机作为其子节点自动跟随),Q/E 改 `CameraComponent.zoom`;随后跑系统、解析相机、产出 RenderView 并桥接提交。把原直接驱动 `tileRenderer` / `batch` 的段替换为:
```cpp
        // 1) 运行时逻辑:直调引擎 API 改实体(高频路径)。
        me::Transform2D pt = scene.LocalTransform(player);
        pt.position += moveDelta; // moveDelta 由 WASD 输入算出(沿用现有输入代码)
        scene.SetLocalTransform(player, pt);
        if (auto* cc = scene.GetComponent<sc::CameraComponent>(cameraEntity)) {
            cc->zoom = zoom; // zoom 由 Q/E 调整(沿用现有变量)
        }

        // 2) 系统:解析世界矩阵 + 活动相机 + 产出 RenderView。
        sc::TransformSystem::UpdateWorldTransforms(scene);
        const auto camView = sc::RenderSystem::ResolveActiveCamera(scene);
        render::OrthographicCamera camera;
        if (camView) {
            camera.SetViewportSize(camView->viewportSize.x, camView->viewportSize.y);
            camera.SetPosition(camView->center);
            camera.SetZoom(camView->zoom);
        }
        const sc::RenderView view = sc::RenderSystem::BuildRenderView(scene);

        // 3) 渲染:地面瓦片 + RenderView 精灵,共享同图集 → 稳定排序保留 Y 序 → 1 drawcall。
        batch->Begin(camera.ViewProj());
        if (auto* tm = scene.GetComponent<sc::TileMapComponent>(tileMapGround)) {
            tileRenderer.Render(*batch, camera, *tm->map, tileset);
        }
        for (const sc::RenderItem& it : view) {
            render::SpriteDesc d;
            d.texture = textureTable[it.textureId];
            d.srcRect = it.srcRect;
            d.dstRect = it.dstRect;
            d.color = it.color;
            d.rotation = it.rotation;
            batch->Submit(d);
        }
        batch->End(cmd);
```

> 删除 M3 里直接对单一地图渲染、不经 Scene 的旧渲染段;保留窗口/输入/清屏/描述符堆绑定等骨架。更新窗口标题为 `"MiniEngine M4 — Scene"`。`moveDelta`/`zoom` 用 sandbox 现有 WASD/QE 输入推导(把它们从“直接改相机”改为“改 player 变换 / 改 CameraComponent.zoom”)。

- [ ] **Step 4: 构建并目视确认(Windows)**

> Windows 构建经 WSL interop(见 `windows-dx12-toolchain` 记忆);sandbox 仅 Windows。

Run:
```bash
cmake -S . -B build-win -DME_BUILD_GPU_TESTS=ON && cmake --build build-win --target sandbox
./build-win/bin/sandbox.exe
```
Expected: 开窗显示瓦片地面 + 其上若干精灵物体;WASD 移动 player(相机跟随)、Q/E 缩放、ESC 退出;同层物体按 Y 正确叠压(低 Y 压在高 Y 之上),整体 1 个 drawcall(单图集)。

- [ ] **Step 5: 写 me_scene README**

创建 `engine/scene/README.md`:

```markdown
# me_scene

混合实体模型场景层(纯 CPU,可在 WSL doctest 单测)。namespace `me::scene`。

## 内容(M4)
- **Entity / Scene**:`Entity = Handle<EntityTag>`(index+generation,销毁后旧句柄失效);
  Scene 管理实体生命周期(槽位回收)、父子层级、缓存世界矩阵 + 脏标记、组件存储。
- **Transform 层级**:每实体局部 `Transform2D` + 父/子邻接;`world = local * parentWorld`(行向量)。
- **Component(数据型)**:`SpriteComponent` / `CameraComponent` / `TileMapComponent`;
  存储隐藏在 `IComponentStorage` / `ComponentStorage<T>`(sparse-set,swap-pop)之后。
- **System**:`TransformSystem`(批解析世界矩阵)、`RenderSystem`(收集精灵 → 算 dstRect →
  按层升序、世界 Y 降序稳定排序 → `RenderView`;解析活动相机 → `CameraView`)。

## 关键约定
- **不依赖 RHI/Renderer**:`RenderView` 用 RHI 无关的 `uint32_t textureId` 引用纹理;
  解析为 `GpuTexture*` 的桥接在渲染边界(当前是 sandbox,未来 Engine 层)完成。
- 2.5D 叠压:同层按世界 Y 降序提交(低 Y 后画 = 压在上);demo 用单图集使 SpriteBatch
  稳定纹理排序保留该顺序。
- 禁全局状态:System 为无状态静态方法,显式接收 `Scene&`。

## 依赖
- `me_core`(数学/句柄/断言)、`me_assets`(`TileMapData`)。**不得**依赖 `me_rhi`/`me_renderer`。
```

- [ ] **Step 6: 补 renderer README 桥接说明**

修改 `engine/renderer/README.md`,新增一小节说明 `me::scene::RenderView → SpriteDesc` 的桥接约定(纹理由调用方 `textureId→GpuTexture*` 解析后填入 `SpriteDesc`,与瓦片同帧 Begin/End 内提交,单图集合 1 drawcall)。

- [ ] **Step 7: 回写进度 + ADR**

修改 `docs/PROGRESS.md`:M4 行 ☐→☑;更新「最后更新」(2026-06-20)、「当前阶段」(M4 完成,下一步 M5 Command 中枢)、「一句话现状」(追加 M4 Scene 摘要)、「下一步行动」(指向 M5);在 ADR 摘要表追加 2026-06-20 条目:
- `me_scene` CPU-only 始终构建、`RenderView` 用 RHI 无关 textureId(Scene/Renderer 解耦)。
- 组件存储用 sparse-set 隐藏在 `ComponentStorage` 接口后(保留 ECS 演进路径)。
- 2.5D 叠压用层 + 世界 Y 降序;demo 单图集使 SpriteBatch 稳定排序保留 Y 序。
- `RenderView→SpriteBatch` 桥接暂放 sandbox(Engine 层未建,运行时层代行)。

创建 `docs/architecture/0002-m4-scene-components.md`(沿用 `0001-m3-tilemap.md` 体例),记录上述决策与理由。

- [ ] **Step 8: 跑全量测试确认无回归**

Run:
```bash
cmake --build build --target me_tests && ./build/bin/me_tests
```
Expected: 全部 PASS(含 M0–M4 跨平台用例)。

- [ ] **Step 9: 提交**

```bash
git add engine/scene/include/me/scene/Components.h engine/scene/README.md \
        sandbox/main.cpp sandbox/CMakeLists.txt \
        engine/renderer/README.md docs/PROGRESS.md docs/architecture/0002-m4-scene-components.md
git commit -m "feat(sandbox): M4 Scene/System 驱动的瓦片地面 + 精灵实体演示 + 进度回写

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Self-Review(规划者已核对)

**Spec 覆盖(spec §5 Scene / §13 M4):**
- `Entity` + 2D Transform 父子层级 → Task 1(生命周期)+ Task 2(层级 + 世界矩阵)。
- `Transform2D`(父子 + 脏标记)→ Task 2(`MarkSubtreeDirty` + 惰性 `WorldMatrix`)。
- 组件数据型 + 存储隐藏在 `ComponentStorage` 接口后 → Task 3(`IComponentStorage`/`ComponentStorage<T>` sparse-set)。
- `SpriteRendererComponent`(SpriteHandle/颜色/排序)→ Task 4 `SpriteComponent`(textureId/color/sortLayer)。
- `CameraComponent`(正交)→ Task 5。
- `TileMapComponent`(TilesetHandle + 瓦片网格)→ Task 6(引用 `TileMapData` + textureId)。
- `TransformSystem` → Task 2;`RenderSystem`(收集可见精灵/瓦片,按层/Y 排序产出 `RenderView`)→ Task 4 + Task 6(瓦片经 TileMapRenderer)。
- Scene → Renderer 单向传 `RenderView` 纯数据、Scene 不持有 RHI、可独立测试 → 全程 textureId + Task 1–5 全 WSL doctest。
- 句柄解析点集中在 Renderer/RHI 边界 → Task 6 sandbox 桥接 textureId→GpuTexture*。
- 测试策略「Scene 可单测:Transform 层级、组件增删、System 产出 RenderView(不碰 GPU)」→ Task 1–5 doctest;GPU 目视 → Task 6 sandbox。

**Placeholder 扫描:** 无 TBD/TODO 残留;`RemoveAllComponents` 在 Task 1 明确为占位空实现、Task 3 填充(已注明)。sandbox 现有变量名(窗口尺寸/输入)注明「打开文件确认实际名称替换」——因 M3 sandbox 具体变量名未在本计划上下文固定,执行者须以文件实际为准(已显式提示)。

**类型一致性:**
- `Entity` / `Scene` 方法签名跨 Task 1/2/3/5 一致(`CreateEntity`/`DestroyEntity`/`IsAlive`/`SetLocalTransform`/`WorldMatrix`/`AddComponent`/`ComponentStore`/`SetActiveCamera`)。
- `ComponentStore<T>()`(创建即用)与 `FindStore<T>()`(只查)命名区分明确,Task 3 定义、Task 4/5 使用。
- `SpriteComponent`/`RenderItem`/`CameraView` 字段名跨 Task 4/5/6 一致(`textureId`/`srcRect`/`dstRect`/`sortLayer`/`size`/`pivot`/`zoom`/`viewportSize`/`center`)。
- `RenderSystem::BuildRenderView` / `ResolveActiveCamera` 跨 Task 4/5/6 一致。

**留给执行者注意的点:**
- `WorldMatrix` 递归在本里程碑不增删槽位,重取 `s` 仅为稳健;若后续在解析中改 `m_slots` 需复审。
- sandbox 的 WASD/QE 输入需从「直接改相机」迁移为「改 player 局部变换 / 改 CameraComponent.zoom」;`moveDelta`/`zoom`/窗口尺寸变量以 M3 `sandbox/main.cpp` 实际命名为准。
- `localId` 取值须落在 demo tileset(4 列 3 行,0..11)范围内。
- Windows 构建与 sandbox 仅 Windows;跨平台 Scene 单测在 WSL 即可红绿(`me_scene` 始终构建)。
- M4 不新增 GPU 渲染代码(复用 M2/M3 已 WARP 验证的 SpriteBatch/TileMapRenderer),故不新增 WARP 测试,渲染正确性由 sandbox 目视把关。
```

