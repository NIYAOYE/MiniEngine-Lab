# M5 — Command 中枢 设计

- **日期**:2026-06-20
- **里程碑**:M5(`ICommand` + `CommandStack` + Undo/Redo)
- **上游设计**:[2026-06-17 架构总设计](2026-06-17-miniengine-design.md) §5「Command 中枢」、§6「Tool 执行模型」
- **状态**:已确认,待生成实现计划

## 1. 目标与范围

为编辑期/自动化/未来 Agent 的受控变更提供**可回滚**基础设施。M5 只建命令基础设施 + 三个具体命令,**不引入 JSON / ToolResult / ToolAPI**(留给 M6)。

**交付**:
- `engine/command/` 模块:`CommandResult`、`ICommand`、`CommandStack`、三个具体命令。
- `Scene` 最小侵入扩展:持久 `EntityId` 身份锚定 + 命令专用恢复 API + 类型擦除组件快照。
- CPU-only doctest 单测(WSL 红绿)。

**不在本里程碑**:JSON 边界、JSON Schema 校验、`ToolResult`、`ToolRegistry`/`Tool`、命令合并(coalescing)、命令栈容量上限、`AddComponentCmd` / `PaintTileCmd` / `SaveSceneCmd`、sandbox 交互演示。

### 关键决策(本次 brainstorm 确认)

| 决策 | 选择 | 理由 |
|------|------|------|
| 实体身份如何在 undo/redo 间稳定 | **持久 `EntityId` + 解析**:命令永不存裸 handle,存 `EntityId`,在 execute/undo 时 `Resolve` 为当前存活 handle | 销毁重建后 `generation` 会变,裸 handle 会悬垂;以单调 `EntityId` 锚定可承受 generation 变化,并为 M6+ 序列化铺路 |
| M5 具体命令范围 | `CreateEntityCmd` + `DestroyEntityCmd` + `SetTransformCmd` | create↔destroy↔transform 往返是最强的 undo/redo 证明;destroy 的全子树+组件还原覆盖最硬的 undo 路径 |
| CommandStack 语义 | 标准双栈:execute 成功才入栈并**清空 redo 栈**;**不做**合并、**不设**容量上限 | 最小正确模型;合并/容量等待真实编辑器(M7)有需求再加(YAGNI) |
| 结果/错误模型 | 轻量 `CommandResult { bool ok; std::string message }` + `describe()` 返回 `std::string` | 遵守「不抛异常、可恢复错误用返回值」;不把 JSON 提前拉进 M5,M6 再把它包装为 `ToolResult` |

## 2. 模块结构与依赖

新增 `engine/command/`(STATIC 库,单向依赖 `me_core` + `me_scene`)。CPU-only,可在 WSL 用 doctest 单测,不依赖 DX12/窗口。

```
engine/command/
├─ include/me/command/
│  ├─ CommandResult.h          # { bool ok; std::string message }
│  ├─ ICommand.h               # execute/undo/describe 接口
│  ├─ CommandStack.h           # Undo/Redo 双栈
│  └─ commands/
│     ├─ CreateEntityCmd.h
│     ├─ DestroyEntityCmd.h
│     └─ SetTransformCmd.h
├─ src/
│  ├─ CommandStack.cpp
│  ├─ CreateEntityCmd.cpp
│  ├─ DestroyEntityCmd.cpp
│  └─ SetTransformCmd.cpp
├─ CMakeLists.txt
└─ README.md
```

同步更新:根 `CMakeLists.txt`(add_subdirectory)、`tests/CMakeLists.txt`(新增 `tests/command/`)、`docs/PROGRESS.md`(里程碑勾选 + 决策记录),并在 `docs/architecture/` 追加 `0003-m5-command.md`(ADR 摘要)。

依赖方向保持单向:`command → scene → core`,无反向依赖。

## 3. Scene 改动(身份锚定 = EntityId)

命令**永不持有裸 `Entity` handle**,改持有持久 `EntityId`,在 execute/undo 时解析为当前存活 handle。即使实体被销毁后重建导致 `generation` 变化,`EntityId` 仍然指向同一逻辑实体。

### 3.1 EntityId 与解析

新增到 `me::scene`:

- `using EntityId = std::uint64_t;`,`0` 表示无效;由 `Scene` 内单调递增计数器分配,**永不复用**。
- `Scene::Slot` 增加字段 `EntityId id = 0;`。
- `Scene` 维护 `std::unordered_map<EntityId, std::uint32_t> m_idToIndex;`(id → 槽位 index)。
- `Entity CreateEntity()`(现有):额外分配新 `EntityId` 并登记到 `m_idToIndex`。
- `EntityId IdOf(Entity e) const;`:存活实体返回其 id,否则 `0`。
- `Entity Resolve(EntityId id) const;`:返回当前存活 handle,无对应存活实体返回 `Entity::Invalid()`。
- `Entity CreateEntityWithId(EntityId id);`:**命令专用恢复路径**。用于 create 的 redo 与 destroy 的 undo —— 以原 `id` 重建实体(复用空闲槽位,`generation` 正常推进,id 重新登记)。前置条件:`id != 0` 且当前不存活(`ME_ASSERT`);并更新单调计数器,保证后续 `CreateEntity` 不会与之冲突。

> 不强制恢复同一 `generation`:身份由 `EntityId` 锚定,handle 的 generation 可以变化,命令通过 `Resolve` 始终拿到当前有效 handle。

`DestroyEntity` 现有行为不变,额外:从 `m_idToIndex` 注销被销毁实体(及其整棵子树)的 id。

### 3.2 组件快照(类型擦除)

destroy 的 undo 需还原被销毁实体的全部组件,但组件存储是类型擦除的。引入只读快照:

- 新增接口 `IComponentSnapshot { virtual ~IComponentSnapshot() = default; virtual void RestoreTo(Scene&, Entity) const = 0; };`
- `ComponentStorage<T>` 配套内部 `ComponentSnapshot<T>`:持 `T` 的值拷贝,`RestoreTo(Scene& s, Entity e)` 调 `s.AddComponent<T>(e, value_)`(`T` 已知,可直接调模板)。
- `IComponentStorage` 增 `virtual std::unique_ptr<IComponentSnapshot> Capture(Entity e) const = 0;`:实体有该组件则返回快照,否则 `nullptr`。
- `Scene` 暴露:
  - `std::vector<std::unique_ptr<IComponentSnapshot>> CaptureComponents(Entity e) const;`:遍历 `m_stores`,收集该实体所有组件快照。
  - `void RestoreComponents(Entity e, const std::vector<std::unique_ptr<IComponentSnapshot>>& snaps);`:对每个快照调 `RestoreTo(*this, e)`。

把 `m_stores` 遍历封装在 `Scene` 内,命令不直接触碰私有存储,符合「Tool/命令不绕过门面改底层对象」的约束精神。

## 4. 命令接口

```cpp
namespace me::command {

/// 命令执行结果:不抛异常,可恢复失败以 ok=false + message 返回。
struct CommandResult {
    bool ok = true;
    std::string message;
    static CommandResult Ok(std::string msg = {}) { return { true,  std::move(msg) }; }
    static CommandResult Fail(std::string msg)    { return { false, std::move(msg) }; }
};

/// 可回滚命令接口。execute 用于首次执行与 redo;undo 撤销;describe 供 dry-run 预览。
class ICommand {
public:
    virtual ~ICommand() = default;
    virtual CommandResult execute(me::scene::Scene& scene) = 0;
    virtual CommandResult undo(me::scene::Scene& scene)    = 0;
    virtual std::string   describe() const                 = 0;
};

/// 标准 Undo/Redo 双栈。
class CommandStack {
public:
    /// 执行命令;成功则压入 undo 栈并清空 redo 栈;失败则丢弃、不入栈。
    CommandResult execute(std::unique_ptr<ICommand> cmd, me::scene::Scene& scene);
    CommandResult undo(me::scene::Scene& scene);   // undo 栈空 → Fail
    CommandResult redo(me::scene::Scene& scene);   // redo 栈空 → Fail
    bool        canUndo() const;
    bool        canRedo() const;
    std::size_t undoDepth() const;
    std::size_t redoDepth() const;
    void        clear();
private:
    std::vector<std::unique_ptr<ICommand>> m_undo;
    std::vector<std::unique_ptr<ICommand>> m_redo;
};

} // namespace me::command
```

语义细节:
- `execute(cmd)`:调 `cmd->execute(scene)`;`ok` 则 `m_undo.push_back(move(cmd))` 且 `m_redo.clear()`;否则不入栈,返回该失败结果。
- `undo()`:取 `m_undo` 顶(不先弹出),调 `undo(scene)`;`ok` 才从 `m_undo` 弹出并压入 `m_redo`;否则命令**留在 `m_undo` 不移动**(便于诊断),返回 `Fail`。栈空返回 `Fail`。
- `redo()`:取 `m_redo` 顶(不先弹出),调 `execute(scene)`;`ok` 才从 `m_redo` 弹出并压回 `m_undo`;否则命令留在 `m_redo` 不移动,返回 `Fail`。栈空返回 `Fail`。

## 5. 三个具体命令

所有命令存 `EntityId` 而非裸 handle,execute/undo 入口先 `Resolve`,失活则 `Fail`。

### 5.1 CreateEntityCmd
- 构造:无参(或可选初始 `Transform2D`,默认单位)。
- `execute`:首次 `CreateEntity()` 取得 handle,记录其 `EntityId m_id`;redo 时(`m_id != 0`)走 `CreateEntityWithId(m_id)`。若指定了初始 transform 则 `SetLocalTransform`。返回 `Ok`。
- `undo`:`DestroyEntity(Resolve(m_id))`;`Resolve` 无效则 `Fail`。
- `describe`:`"创建实体 #<m_id>"`。

### 5.2 SetTransformCmd
- 构造:`EntityId target`、新 `Transform2D newLocal`。
- `execute`:`e = Resolve(target)`,失活 → `Fail`;首次执行时缓存 `m_oldLocal = LocalTransform(e)`,再 `SetLocalTransform(e, newLocal)`。返回 `Ok`。
- `undo`:`SetLocalTransform(Resolve(target), m_oldLocal)`;失活 → `Fail`。
- `describe`:`"设置实体 #<target> 局部变换 (pos/rot/scale 新旧值)"`。

### 5.3 DestroyEntityCmd
- 构造:`EntityId target`。
- `execute`(快照整棵子树后销毁):
  1. `e = Resolve(target)`,失活 → `Fail`。
  2. 以 `e` 为根**前序遍历**子树,对每个实体记录 `EntitySnapshot { EntityId id; Transform2D local; EntityId parentId; std::vector<std::unique_ptr<IComponentSnapshot>> comps; }`(`parentId` 取自 `Parent` 的 `IdOf`,根的父若存在也记录;`comps` 来自 `Scene::CaptureComponents`)。
  3. 记录 `bool m_wasActiveCamera = (ActiveCamera 在被销毁子树内)` 及其 `EntityId`。
  4. `DestroyEntity(e)`。返回 `Ok`。
- `undo`(按原 id 重建):
  1. 按记录顺序(父先于子)对每个快照 `CreateEntityWithId(id)`。
  2. `SetLocalTransform` 还原局部变换。
  3. 用 `parentId` 经 `Resolve` 重连父子(`SetParent`);`parentId==0` 则无父。
  4. `RestoreComponents` 还原组件。
  5. 若 `m_wasActiveCamera`,`SetActiveCamera(Resolve(cameraId))`。
  - 快照在命令对象内移动持有;重建后保留(支持再次 redo→undo,redo 会重新销毁,undo 再次重建)。
- `describe`:`"销毁实体 #<target> 及其子树(<N> 个实体)"`。

> 前序遍历 + 父先子后重建,保证 `SetParent` 时父已存在。`EntityId` 作为父子重连的稳定键,避免依赖 handle 顺序。

## 6. 测试与验收

`tests/command/`(CPU-only doctest,WSL 红绿):

- **CreateEntityCmd**:execute 后实体存活 → undo 后失活 → redo 后再存活;redo 后的实体 `EntityId` 与原一致(`Resolve` 命中)。
- **SetTransformCmd**:execute 改变 `LocalTransform` → undo 还原旧值 → redo 再变;对失活实体 execute 返回 `Fail`。
- **DestroyEntityCmd**:构造「父-子-孙 + 组件 + active camera」场景;execute 后整树失活 → undo 后**层级、局部变换、组件、active camera 全部还原**(用 `EntityId` 解析校验);redo 再次销毁,二次 undo 仍正确。
- **CommandStack 语义**:execute 成功后 `undoDepth` 增、`redoDepth` 归零;undo 后可 redo;**新 execute 清空 redo 栈**;空栈 undo/redo 返回 `Fail`;execute 失败的命令不入栈。

`tests/scene/` 补充 Scene 新增 API 单测:`IdOf`/`Resolve` 往返、`CreateEntityWithId` 复用 id、`CaptureComponents`/`RestoreComponents` 还原。

**验收标准**:上述单测全绿;`command → scene → core` 依赖单向(CMake 构建图);无硬编码数值;公开 API 有 Doxygen 注释。

## 7. 不做 / 推迟(YAGNI)

- JSON 边界、JSON Schema 校验、`ToolResult`、`ToolRegistry`/`Tool`/`ToolContext` → M6。
- 命令合并(drag 多次 SetTransform 折叠)、命令栈容量上限 → 待 M7 交互编辑器有需求时。
- `AddComponentCmd` / `PaintTileCmd` / `SaveSceneCmd` → 随对应能力(组件编辑 / 瓦片编辑 / 序列化)落地。
- sandbox 交互演示(按键 create/destroy/undo/redo)→ M7 编辑器接 UI 时一并做;M5 以单测验收为准。

## 8. 开放问题

- `describe()` 当前返回中文人读字符串;M6 接 dry-run 时是否改为结构化(JSON diff)在 M6 决定。
- 组件快照目前每命令深拷贝;若后续组件体积变大需考虑写时复制,推迟到出现性能问题再议。
