# M7 Editor as Client — 设计文档

- **日期**:2026-06-21
- **里程碑**:M7 Editor as Client(精简切片)
- **前置**:M6 ToolAPI 完成(6 Tool:3 query + 3 mutation,经 CommandStack 可 Undo)
- **状态**:设计已确认,待写实现计划

## 1. 目标与范围

### 1.1 目标
M7 的唯一目标:**用引擎自己的编辑器作为 M6 Tool API 的第一个客户端,反向验证接口完备性**。当前阶段不接大模型、不实现 Agent,只建设并验证接口层。

### 1.2 核心设计原则
**EditorController 只经 `ToolRegistry::Invoke` 与引擎对话——读也走查询 Tool、写也走变更 Tool。**

这是 M7 区别于"随便写个 ImGui 面板直接调 Scene"的关键:若某个面板需要的数据查询 Tool 给不出来,那就是 Tool API 的缺口——而暴露并记录缺口正是本里程碑的价值。CallerRole 固定为 `CallerRole::Editor`(最高权,可调用全部 6 Tool)。

> 注:项目设计哲学"只读展示直接查询"允许面板直接读 Scene。本里程碑**主动选择更严格的"读也走 Tool"**,因为反向验证是 M7 的存在理由;直接读 Scene 会绕过被验证的对象,使本里程碑失去意义。
>
> 边界:该原则覆盖的是**场景/实体编辑状态**(实体列表、实体详情、审计日志),这些必须经查询 Tool。Stats 面板的**运行时渲染遥测**(drawcall 数、帧时)不属于场景编辑状态、也无对应 Tool,直接从运行时统计读(见 §4.4),不构成对本原则的违反。

### 1.3 范围(精简切片)
**只使用现有 6 个 Tool,零新增 Tool、零序列化。**

| 纳入 | 不纳入(后续切片) |
|------|----------|
| Hierarchy 面板(`scene.list_entities`) | TileMap 编辑面板(需 `tilemap.paint` + PaintTile 命令) |
| Inspector 面板(`scene.get_entity` + `entity.set_transform`) | 存档 save/load(需 `scene.save`/`scene.load` + 场景序列化) |
| Create / Destroy 按钮(`scene.create_entity`/`scene.destroy_entity`) | `entity.add_component` 等组件编辑 Tool |
| Undo / Redo 按钮(直调 CommandStack,见 §5) | Engine/Application/Layer 栈抽象 |
| Stats 面板(实体数 / drawcall / 帧时,只读) | Agent / LLM 接入 |
| Log 面板(`log.read`) | |

YAGNI 依据:`create → set_transform → destroy → undo` 往返是接口完备性最强证明,与 ADR 0003/0004 一脉相承。新 Tool 依赖未落地子系统(瓦片编辑数据模型、场景序列化),推迟到需求明确的后续切片。

## 2. 架构与分层

### 2.1 模块划分
```
engine/editor/  → me_editor 静态库(CPU-only,始终构建)
                  依赖:editor → toolapi → command → scene → core + nlohmann/json
                  不依赖 ImGui、不依赖 RHI ⇒ 可在 WSL doctest 红绿
third_party/    → 新增 Dear ImGui(FetchContent 钉 tag;
                  编译 imgui 核心 + imgui_impl_dx12 + imgui_impl_win32)。仅真机构建
sandbox/        → 现有 DX12 主循环接入 ImGui 后端 + 面板绘制;
                  ImGui 封死在 sandbox/app 边界(同 RHI、同 RenderView→SpriteBatch 桥接)
```

### 2.2 依赖单向性
`me_editor` 严格只向下依赖 `me_toolapi`(进而 command/scene/core)。**ImGui 不进入 me_editor**:面板是目视层,留在 sandbox。这保证:
- me_editor 与 RHI/GPU/窗口完全解耦,WSL 可单独编译单测;
- ImGui 依赖与 DX12 一样封死在应用边界,不外泄到引擎库。

理由同 M4 ADR「`RenderView→SpriteBatch` 桥接暂放 sandbox」:当桥接逻辑增长到值得提取时再迁移,不提前造 Engine 层。

## 3. me_editor 内部设计

### 3.1 EditorController
唯一的可测核心。持有(引用,不持有所有权):
- `me::toolapi::ToolRegistry&`
- `me::toolapi::ToolContext&`(内含 `Scene&` / `CommandStack&` / `ToolInvocationLog&`)

持有 UI 状态:
- 当前选中 `EntityId`(无选中时为无效哨兵)
- 最近错误消息 `std::string m_lastError`
- 刷新缓存的 DTO 列表(Hierarchy 行、Log 行)

固定角色:所有 Invoke 用 `CallerRole::Editor`。

### 3.2 意图方法 → Tool 调用映射

| EditorController 方法 | 调用 Tool | 结果处理 |
|------|------|------|
| `RefreshHierarchy()` | `scene.list_entities` | 解析 JSON → `std::vector<EntityRow>` |
| `Select(EntityId)` | (本地状态,无 Tool) | 记录选中;触发 `InspectSelected()` |
| `InspectSelected()` | `scene.get_entity` | 解析 JSON → `EntityDetails` |
| `CreateEntity()` | `scene.create_entity` | 成功后刷新 Hierarchy;选中新实体 |
| `DestroySelected()` | `scene.destroy_entity` | 成功后清空选中 + 刷新 Hierarchy |
| `ApplyTransform(const Transform2D&)` | `entity.set_transform` | 成功后刷新 Inspector |
| `RefreshLog()` | `log.read` | 解析 JSON → `std::vector<LogRow>` |
| `Undo()` / `Redo()` | (直调 CommandStack,见 §5) | 成功后刷新 Hierarchy/Inspector |

### 3.3 DTO 与 JSON 解析
DTO 结构集中在 me_editor,面板只读 DTO、不碰 JSON:
- `EntityRow { EntityId id; std::string label; }`
- `EntityDetails { EntityId id; Transform2D transform; std::vector<std::string> components; bool hasTransform; }`
- `LogRow { std::uint64_t invocationId; std::string toolName; std::string status; }`

JSON → DTO 的解析逻辑全部在 controller(对照 `scene.get_entity` 等 Tool 的实际 JSON 输出形状实现)。解析失败(字段缺失/类型不符)按错误处理:写 `m_lastError`,不崩、不抛(遵守项目不用异常规范)。

### 3.4 错误处理
每次 `Invoke` 返回的 `ToolResult` 非 ok 时:
- 写入 `m_lastError`(含五码错误模型的 code + message);
- 面板渲染红字;
- **绝不静默忽略**(契合 ToolAPI 受控性与 CLAUDE.md 错误处理规则)。

## 4. sandbox 集成(目视层)

### 4.1 ImGui 后端接入
- third_party 引入 Dear ImGui;sandbox 链接 imgui 核心 + `imgui_impl_dx12` + `imgui_impl_win32`。
- ImGui DX12 后端的字体纹理需要一个 SRV 描述符堆槽位:复用现有 `srvHeap`(容量 8,当前仅用 1 槽,余量充足),为 ImGui 字体分配一槽。
- 初始化:`ImGui::CreateContext` → `ImGui_ImplWin32_Init(hwnd)` → `ImGui_ImplDX12_Init(...)`。

### 4.2 Win32 消息钩子
`imgui_impl_win32` 需要看到窗口消息(`ImGui_ImplWin32_WndProcHandler`)。`platform::Window` 的 WndProc 当前不转发给外部。**新增一个消息钩子**:Window 提供注册外部 WndProc 回调的能力,sandbox 注册 ImGui 的处理器。小改动,作为集成任务;保持 platform 层不依赖 ImGui(回调签名用裸 Win32 类型,ImGui 处理器在 sandbox 侧适配)。

### 4.3 每帧顺序
```
1. input.NewFrame() + window->PumpMessages()   (Win32 消息;ImGui 经钩子收到)
2. 游戏逻辑:现有 WASD 运行时直调 Scene(高频路径,不经 Tool)
3. ImGui_ImplDX12_NewFrame + ImGui_ImplWin32_NewFrame + ImGui::NewFrame
4. 面板绘制:Hierarchy / Inspector / Stats / Log
     面板读 EditorController 的 DTO,按钮/控件调 controller 意图方法
     controller → ToolRegistry::Invoke → CommandStack → Scene 变更
5. Systems:TransformSystem::UpdateWorldTransforms + RenderSystem(在编辑变更之后跑)
6. GPU:清屏 → 渲染场景(瓦片 + RenderView 精灵)→ ImGui::Render
        + ImGui_ImplDX12_RenderDrawData 录到同一命令列表 → Present
```

### 4.4 面板内容
- **Hierarchy**:列出 `EntityRow`,点击 = `Select`。底部 Create 按钮。
- **Inspector**:显示选中实体 `EntityDetails`;transform 三组控件(position/rotation/scale),编辑后点 Apply = `ApplyTransform`;Destroy / Undo / Redo 按钮。
- **Stats**:实体数、drawcall 数、帧时(只读,直接从运行时统计读,非 Tool 数据)。
- **Log**:列出 `LogRow`(invocationId / toolName / status),刷新按钮 = `RefreshLog`。
- 编辑器整体可一键开关(如 `` ` `` 键),关闭后纯游戏画面。

## 5. 反向验证发现(M7 产出)

精简切片**不补 Tool**,但显式记录验证过程暴露的 Tool API 缺口,作为 M7 交付物之一(回写 PROGRESS + ADR):

1. **Undo/Redo 未暴露为 Tool**:M6 的 CommandStack 在 ToolContext 内,但没有 `edit.undo`/`edit.redo` Tool。精简切片内,编辑器作为特权宿主**直接调 `ctx.commands.Undo()/Redo()`**。标注为发现的缺口 → 后续候选 Tool `edit.undo` / `edit.redo`(届时 Agent 也能受控撤销)。
2. **实体人类可读 label**:若 `scene.list_entities` / `scene.get_entity` 只回 EntityId,Hierarchy 只能显示裸 id。本切片按 Tool 实际返回字段显示(有 label 则用,否则显示 id)。标注为候选缺口 → 后续 `entity.set_name` / get_entity 增 name 字段。

> 这些发现正是"Editor as Client 反向验证接口完备性"的具体兑现,而非实现瑕疵。

## 6. 测试策略

### 6.1 me_editor:WSL doctest(全 CPU-only)
用真实 `Scene` + `CommandStack` + `ToolRegistry`(注册 builtins)+ `ToolContext` 驱动 `EditorController`,断言:

| 场景 | 断言 |
|------|------|
| 创建实体后 RefreshHierarchy | 行数/内容匹配 |
| Select + InspectSelected | transform/组件列表正确 |
| CreateEntity 意图 | 实体数 +1,Hierarchy 刷新含新实体,新实体被选中 |
| ApplyTransform | 随后 get_entity 反映新 transform |
| DestroySelected | 实体消失,选中清空 |
| Undo(create 之后) | 实体被移除 |
| 错误参数 Invoke | `LastError()` 被填充,无崩溃 |
| 若干 Invoke 后 RefreshLog | 审计条目存在(log.read) |

**这组测试即"6 个 Tool + JSON 结果足以驱动完整编辑器"的证明。**

### 6.2 sandbox:真机目视
ImGui 面板交互、画面叠加、Undo 后画面回退等,靠 sandbox 真机目视确认(同 RHI 层处置:不强求自动化)。

## 7. 交付物清单
- `engine/editor/`:me_editor 静态库(EditorController + DTO + JSON 解析)+ CMakeLists。
- `tests/editor/`:EditorController doctest(§6.1)。
- `third_party/`:Dear ImGui FetchContent 接入(钉 tag)。
- `platform::Window`:WndProc 外部消息钩子(§4.2)。
- `sandbox/main.cpp`:ImGui 初始化 + 每帧接入 + 四面板绘制。
- 文档:ADR 0005 + 回写 `docs/PROGRESS.md`。

## 8. 未决 / 后续切片
- TileMap 编辑面板:依赖 `tilemap.paint` Tool + PaintTile 命令 + 可编辑 tile 数据模型。
- 存档 save/load:依赖场景序列化(独立子项目)+ `scene.save`/`scene.load` Tool。
- `edit.undo` / `edit.redo` Tool(由 §5 发现驱动)。
- Engine/Application/Layer 栈:当 sandbox 主循环增长到值得提取时再建。
