# ADR 0003 — M5 Command 中枢架构决策

- **状态**: 已接受
- **日期**: 2026-06-21
- **里程碑**: M5 Command 中枢

---

## 背景

M5 目标是为编辑期/自动化/未来 Agent 的受控变更建立**可回滚**基础设施:`ICommand`(execute/undo/describe)+ `CommandStack`(Undo/Redo)+ 首批具体命令(创建实体 / 改 Transform / 销毁实体)。M5 只建命令基础设施,不引入 JSON / ToolResult / ToolAPI(留给 M6)。需要决策四件事:实体身份如何在 undo/redo 间稳定、命令具体范围、CommandStack 语义、结果/错误模型。新增 `engine/command/` 静态库,单向依赖 `me_scene → me_core`,CPU-only 可在 WSL 独立单测。

---

## 决策一:命令以持久 `EntityId` 锚定身份(非裸 handle)

**决策**:`Scene` 新增持久 `EntityId`(`std::uint64_t`,0 无效,单调递增、永不复用)。命令永不持有裸 `Entity` handle,改持有 `EntityId`,在 execute/undo 时经 `Scene::Resolve(EntityId)` 解析为当前存活 handle。配套 `Scene::IdOf`、`Scene::CreateEntityWithId`(命令恢复路径:create 的 redo、destroy 的 undo 以原 id 重建)。

**理由**:
- 现有 `Entity` 是 `index+generation`,销毁后重建会令 generation 变化,裸 handle 立即悬垂;
- 以单调 `EntityId` 锚定逻辑身份,可承受 generation 变化,命令通过 `Resolve` 始终拿到当前有效 handle;
- 为 M6+ 场景序列化(存档需要稳定的实体引用)预先铺路。

**替代方案**:命令存裸 handle 并要求 redo 复用同一 generation — 需强制回退 generation,侵入 handle 不变量,且无法跨序列化。

---

## 决策二:M5 命令范围 = Create / Destroy / SetTransform

**决策**:本里程碑实现 `CreateEntityCmd`、`DestroyEntityCmd`、`SetTransformCmd` 三个命令。`PaintTileCmd` / `AddComponentCmd` / `SaveSceneCmd` 推迟到对应能力(瓦片编辑 / 组件编辑 / 序列化)落地。

**理由**:
- create↔destroy↔transform 往返是最强的 undo/redo 正确性证明;
- `DestroyEntityCmd` 的全子树 + 组件 + active camera 还原覆盖最硬的 undo 路径,验证身份锚定与组件快照机制;
- 其余命令依赖尚未存在的能力,提前实现是投机(YAGNI)。

**替代方案**:照搬 spec 首批清单一次性实现五个命令 — 引入对未落地能力的依赖,扩大里程碑、增加一次性出错面。

---

## 决策三:CommandStack 标准双栈;新 execute 清空 redo;不合并/不设上限

**决策**:`CommandStack` 维护 undo/redo 双栈。`execute` 成功才接管所有权入 undo 栈并清空 redo 栈;失败丢弃命令、不入栈、无副作用。undo/redo 采用 **peek-then-pop**:先在栈顶命令上执行,仅成功才移动到另一栈,失败保留原位便于诊断。空栈返回 `Fail`。不做命令合并(coalescing),不设容量上限。

**理由**:
- 标准双栈是最小正确模型,语义直观可学;
- peek-then-pop 保证失败不破坏栈结构;
- 命令合并(拖拽多次 SetTransform 折叠)与容量上限要待真实交互编辑器(M7)有需求时再加,提前引入增加复杂度且无消费方。

**替代方案**:execute 时即合并相邻同类命令 — 在无编辑器交互场景下是无谓复杂度。

---

## 决策四:轻量 `CommandResult` + 字符串 `describe()`(不抛异常,不提前引入 JSON)

**决策**:`execute`/`undo` 返回 `CommandResult { bool ok; std::string message }`,`describe()` 返回人读 `std::string`(供 dry-run/日志)。不在 M5 引入 JSON / `ToolResult`。

**理由**:
- 遵守 CLAUDE.md「不抛异常,可恢复错误用返回值」;
- 把 JSON 边界与结构化错误模型留在 M6,避免 M5 过早耦合 nlohmann/json;
- `CommandResult` 可被 M6 直接包装为 JSON `ToolResult`,平滑衔接。

**配套实现说明(避免头文件循环)**:类型擦除组件快照 `IComponentSnapshot::RestoreTo(Entity)` 经快照内持有的 `ComponentStorage<T>*` 回写,而非回调 `Scene` —— 否则 `Scene.h ↔ ComponentStorage.h` 形成包含环。相应 `Capture`/`CaptureComponents` 为非 const(销毁命令本就在非 const Scene 上工作)。

---

## 后果

- 新增 `engine/command/` 静态库(`me_command → me_scene → me_core`),全程 CPU-only,WSL doctest 红绿(M5 收尾全套 101/101)。
- `Scene` 获得持久身份层(`EntityId`/`IdOf`/`Resolve`/`CreateEntityWithId`)与类型擦除组件快照(`CaptureComponents`/`RestoreComponents`),为 M6 Tool API 的变更入口与未来序列化复用。
- 顺带修复一处 Scene 既有不变量缺口:`Scene::DestroyEntity` 现会清除被销毁(子树内)实体上的悬垂 `m_activeCamera`,使任意销毁路径都不再留下悬垂活动相机句柄(命令层不再代偿)。
- M6 起:变更型 Tool 经 `CommandStack` 执行,`CommandResult` 包装为 JSON `ToolResult`;命令合并/容量上限待 M7 交互编辑器。
- 已知后续项(非阻塞):`Handle::IsValid()` 仅校验 index 不校验 generation —— 悬垂 handle 仍报 valid,是更深层设计权衡,本里程碑不动,留待后续统一。
