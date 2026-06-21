# ADR 0004 — M6 ToolAPI 层架构决策

- **状态**: 已接受
- **日期**: 2026-06-21
- **里程碑**: M6 ToolAPI

---

## 背景

M6 目标是把引擎编辑能力抽象为**统一、受控、可验证**的 Tool API:编辑器、自动化、未来 Agent 都经同一套接口调用引擎能力。新增 `engine/toolapi/` 静态库,单向依赖 `me_command → me_scene → me_core`(+ nlohmann/json 作为边界格式),全程 CPU-only 可在 WSL 独立单测。本里程碑明确**不接大模型、不建编辑器**(M7),只建接口层本身:`ITool` 接口、`ToolRegistry` 统一流水线、`ToolContext` 受控门面、参数 JSON Schema 校验、dry-run 预览、白名单权限、`ToolInvocation` 审计日志、结构化 `ToolResult` 错误模型。需要决策五件事:Schema 校验如何实现、首批 Tool 范围、受控边界形态、变更如何获得 Undo、权限与审计模型。

---

## 决策一:JSON Schema 校验手写最小子集(不引第三方库)

**决策**:手写一个递归的 JSON-Schema **子集**校验器 `ValidateAgainstSchema(schema, params) → ValidationResult{ok, errors[]}`,支持 `type`(object/array/string/number/integer/boolean)、`required`、`properties`(递归进嵌套对象)、`minimum`/`maximum`(number/integer)、`enum`(string)。`params` 顶层须为对象;未声明的多余字段宽松放行。`integer` 拒绝小数、`number` 接受整数。

**理由**:
- 契合 CLAUDE.md「最小依赖」与「不抛异常」(校验返回值而非异常);
- 首批 Tool 的 schema 都很简单(几个具名字段 + 范围/枚举),手写够用;
- Schema 本身是 nlohmann::json,可序列化,天然适配未来 LLM 工具调用的 schema 暴露。

**替代方案**:FetchContent 引入 pboettch/json-schema-validator 等 — 增加依赖、多数特性用不上,部分库依赖异常,与项目规范冲突。

---

## 决策二:首批 Tool 聚焦已落地能力(3 query + 3 mutation)

**决策**:本里程碑实现六个 Tool —— 查询型 `scene.list_entities` / `scene.get_entity` / `log.read`;变更型(经 Command)`scene.create_entity` / `scene.destroy_entity` / `entity.set_transform`(复用 M5 的 `CreateEntityCmd` / `DestroyEntityCmd` / `SetTransformCmd`)。设计文档首批清单中的 `entity.add_component` / `tilemap.paint` / `scene.save` / `scene.load` 推迟到对应底层能力(组件编辑命令 / 瓦片编辑命令 / 场景序列化)落地。

**理由**:
- create→set_transform→destroy→undo 往返是接口层完备性的最强证明:覆盖完整流水线 + 白名单 + dry-run + Undo;
- 其余 Tool 依赖 M5 已显式推迟或尚未存在的能力,提前实现是投机(YAGNI);
- 与 ADR 0003 决策二一脉相承(命令范围同样聚焦已落地能力)。

**替代方案**:照搬 spec 首批清单一次性实现全部 Tool — 跨越多个未建子系统,扩大里程碑、增加一次性出错面。

---

## 决策三:`ToolContext` 是 Tool 访问引擎的唯一受控边界

**决策**:Tool 无状态,只经注入的 `ToolContext{ Scene& scene; CommandStack& commands; ToolInvocationLog& log }` 访问引擎,拿不到任何全局指针。查询型 Tool 直接读 `ctx.scene`;变更型 Tool 只经 `ctx.commands.execute(...)` 落地;`log.read` 读 `ctx.log`。`ITool.h` 前置声明 `ToolContext`(仅按引用使用),把 `Scene.h`/`CommandStack.h` 的包含封死在 `ToolContext.h`,使工具实现头不被迫拉入场景/命令子系统。

**理由**:
- 契合 CLAUDE.md「禁 Singleton / 禁全局可变状态 / 依赖注入」;
- 受控边界使「Tool 能触达什么」显式可审,为未来 Agent 白名单收紧留接口;
- 前置声明隔离降低编译耦合,保持依赖图清爽。

**替代方案**:Tool 直接持有引擎子系统指针或全局访问 — 违反架构约束,且无法约束 Tool 能力面。

---

## 决策四:变更型 Tool 必经 CommandStack,天然获得 Undo

**决策**:变更型 Tool 的 `run` **只**通过构造 M5 `ICommand` 并 `ctx.commands.execute(...)` 改场景,绝不直接调 `Scene::CreateEntity/DestroyEntity/SetLocalTransform`。命令失败映射为 `ToolErrorCode::ExecutionFailed`。`dryRun` 零副作用:create 仅返回预览串;destroy/set_transform 先校验实体存活(不存在返回 `PreconditionFailed`)再返回预览,不执行命令。

**理由**:
- 变更经 CommandStack → 自动获得 Undo/Redo,这是 ToolAPI 区别于直调引擎的核心受控性;
- dry-run + Command 回滚 + 审计日志 = 「能力可预览、可回滚、可审计」的 Agent-ready 安全基础;
- 衔接 ADR 0003:`CommandResult` 在 Tool 边界被包装为 `ToolResult`。

**替代方案**:Tool 直接改 Scene 再另设撤销机制 — 绕过既有 CommandStack,重复造轮子且破坏单一变更通道。

---

## 决策五:三层权限白名单 + 全路径审计(成功与失败都记录)

**决策**:`CallerRole{Agent, Automation, Editor}` 与 `Permission{AgentAllowed, Automation, EditorOnly}` 按枚举序分层,`IsAllowed(role, perm)` 即整型比较(角色特权 ≥ 工具要求)。首批权限分配刻意覆盖三层:查询型 = `AgentAllowed`;`create_entity` / `set_transform` = `Automation`;`destroy_entity`(破坏性)= `EditorOnly`。`ToolRegistry::Invoke` 统一流水线六步:(1) 查找→UnknownTool;(2) 白名单→PermissionDenied;(3) Schema 校验→InvalidParams(errors 入 data);(4) dryRun? 预览;(5) run 落地;(6) **每个结果(含三类错误返回)都写 `ToolInvocationLog` 并回填 `invocationId`**。`ToolInvocation` 可序列化为 JSON。

**理由**:
- 分层权限以最小机制演示完整白名单裁决,为未来 Agent 只调 `agent-allowed` 收口铺路;
- 全路径(含失败)记录保证审计完整 —— 被拒绝/非法的调用同样留痕,绝不静默;
- `ToolResult{ok, code, message, data, invocationId}` 五码错误模型(UnknownTool/PermissionDenied/InvalidParams/PreconditionFailed/ExecutionFailed)统一结构化返回,不抛异常。

**替代方案**:仅记录成功调用 / 布尔式 allow-deny — 审计有盲区,且无法表达分层与未来 Agent 收紧。

---

## 后果

- 新增 `engine/toolapi/` 静态库(`me_toolapi → me_command → me_scene → me_core` + nlohmann/json PUBLIC),全程 CPU-only,WSL doctest 红绿(M6 收尾全套 129/129)。
- 引擎获得受控变更入口:编辑期/自动化/未来 Agent 经同一 `ToolRegistry.Invoke` 调六个 Tool;变更经 CommandStack 可 Undo,调用经 `ToolInvocationLog` 可审计,dry-run 可预览。
- `RegisterBuiltinTools(ToolRegistry&)` 一次注册全部首批 Tool,供 M7 编辑器作为首个客户端反向验证接口完备性。
- 已解决 M5 遗留开放问题:JSON Schema 校验选型 = 手写最小子集。
- 已知后续项(非阻塞):`Make*Tool` 工厂定义行无 inline Doxygen(文档统一在 `BuiltinTools.h` 声明的 `///<`,六工厂一致);schema `minimum:1` 字面量为 EntityId≥1 域约束(与 `get_entity` 一致,可统一抽常量);校验器嵌套 `required` 错误消息 framing 与 `field '...'` 略不一致(M6 无工具用嵌套 required,纯 cosmetic)。
- M7 起:ImGui 面板(Hierarchy/Inspector/TileMap)改为经 ToolAPI 操作;`add_component` / `tilemap.paint` / `scene.save` / `scene.load` 等 Tool 待对应底层能力落地后补齐。
