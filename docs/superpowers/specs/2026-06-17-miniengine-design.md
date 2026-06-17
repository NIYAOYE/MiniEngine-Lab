# MiniEngine 架构设计文档 —— 2D/2.5D 农场模拟引擎(Agent-ready)

- **日期**:2026-06-17(数次调整:Agent-ready Tool API → 融合回归 2D/2.5D 农场模拟)
- **状态**:已与用户确认
- **类型**:学习型 **2D/2.5D 农场模拟**游戏引擎,面向「可被工具/自动化/未来 Agent 受控调用」的 API 设计

## 1. 项目目标与约束

MiniEngine 是一个从零开始、以**学习和掌握游戏引擎原理**为首要目标的引擎,品类定位为**星露谷物语类 2D/2.5D 农场模拟游戏**。在引擎底座之上确立一个核心特色:**将引擎编辑能力抽象为统一、受控、可验证的 Tool API**,使编辑器、脚本、自动化测试与未来的 Editor Agent 都能通过同一套接口调用引擎能力。

**当前阶段明确不实现完整 Agent、不接入大模型**,只建设接口层本身(Tool Registry、Command/Undo、参数校验、执行日志、错误返回、dry-run 预览),并以引擎自身的编辑器作为第一个客户端来反向验证接口完备性。农场领域专用模块(时间系统、瓦片地图玩法、NPC 日程等)作为**后期里程碑的领域层**。

| 维度 | 决策 |
|------|------|
| 引擎类型 | 2D/2.5D 农场模拟游戏引擎(渲染 + 场景/实体 + 资源 + 输入 + 后期农场领域层/物理) |
| 语言 | C++17(可逐步引入 C++20) |
| 图形 API | DirectX 12(用于 2D 渲染:精灵/瓦片图集、正交相机) |
| 平台 | Windows(Win32) |
| 维度 | **2D/2.5D**(精灵 + 瓦片地图 + 正交相机) |
| 游戏逻辑 | 运行时纯 C++ 代码直调;编辑期变更走 Tool API + Dear ImGui 调试 GUI |
| 首要原则 | **代码清晰可读 > 极致性能**;接口稳定、模块解耦、可独立理解与测试 |
| 受控原则 | 编辑期对场景/资源的修改必须**可校验、可预览、可审计、可回滚** |
| 数据驱动 | 地图、物品、配方、对话、NPC 行为等内容从外部 JSON 加载,不硬编码 |

**非目标(YAGNI)**:当前阶段不实现完整 Agent、不接入 LLM、不做 3D、不做脚本语言绑定、不追求生产级性能优化。

## 2. 实体/场景模型决策

采用**混合模型(方案 C)**:OOP 场景层级 + 轻量数据型组件 + System 处理。

- 以 `Entity`(含 2D Transform 父子层级)为骨架,概念贴近 Unity,直观好学。
- 组件尽量是**纯数据 + 极少逻辑**;更新逻辑放在 System 中,由 System 遍历组件处理。
- 组件存储隐藏在 `ComponentStorage` 接口之后,**后期可演进为纯 ECS 存储而不破坏上层**。
- **不引入 EnTT**:ECS 思想用自实现的混合模型,契合学习目标。

## 3. 分层架构

核心原则:**自下而上单向依赖**,上层依赖下层,下层绝不反向依赖上层。每层职责单一、可独立理解和测试。依赖方向由 CMake 构建图强制。

引擎底座之上新增 **Command 中枢** 与 **ToolAPI 层**(编辑期/自动化/未来 Agent 的唯一受控变更入口);农场玩法在 **Domain 领域层**(后期里程碑)。

```
┌──────────────────────────────────────────────────────────────┐
│  Sandbox / Game        运行时:直接调用 C++ 引擎 API(高频/性能)   │
├──────────────────────────────────────────────────────────────┤
│  Domain 农场领域层 (后期里程碑)  时间系统/瓦片地图玩法/作物/         │
│                                  库存物品/NPC 日程(数据驱动)       │
├──────────────────────────────────────────────────────────────┤
│  Editor / Automation / (未来)Agent   ← 只通过 Tool API 做编辑变更 │
├──────────────────────────────────────────────────────────────┤
│  ToolAPI 层                                                    │
│   ├─ ToolRegistry      统一注册/发现/白名单/权限                  │
│   ├─ Tool (接口)        name + JSON Schema + validate/dryRun/run │
│   ├─ ToolContext        受控访问引擎子系统的唯一入口(注入)         │
│   └─ ToolInvocation     执行记录:输入/结果/错误/diff/日志         │
├──────────────────────────────────────────────────────────────┤
│  Command 中枢                                                  │
│   ├─ ICommand          execute / undo / describe(dry-run)       │
│   ├─ CommandStack       Undo/Redo 栈                            │
│   └─ 具体命令           CreateEntityCmd / SetTransformCmd / ...   │
├──────────────────────────────────────────────────────────────┤
│  Engine (应用层)        主循环、子系统编排、Application            │
├──────────────┬───────────────┬──────────────┬─────────────────┤
│  Scene        │  Renderer(2D) │  Physics(2D)  │  (Editor 已上移   │
│  (实体/组件/    │  精灵/瓦片图/   │  (后期里程碑)   │   为 ToolAPI 客户端)│
│   System)     │  正交相机/批渲染│  AABB/碰撞     │                 │
├──────────────┴───────────────┴──────────────┴─────────────────┤
│  Assets         资源加载/缓存/生命周期(Texture/Atlas/Tileset/    │
│                 Sprite/内容JSON),句柄化访问,导入器               │
├──────────────────────────────────────────────────────────────┤
│  RHI            DX12 薄封装(Device/CmdList/Buffer/Texture/      │
│                 PSO/Descriptor/Fence 同步)                       │
├──────────────────────────────────────────────────────────────┤
│  Platform       窗口、输入、计时、文件、动态库(Win32)              │
├──────────────────────────────────────────────────────────────┤
│  Core           数学库(2D)、容器、日志、断言、内存、事件、句柄、类型id│
└──────────────────────────────────────────────────────────────┘
```

依赖边(单向):
- `Core` ← 零外部依赖。
- `Platform` → Core。
- `RHI` → Core, Platform。
- `Assets` → Core, Platform, RHI。
- `Scene` → Core, Assets。
- `Renderer` → Core, RHI, Assets。
- `Engine` → Scene, Renderer, Assets, RHI, Platform, Core。
- `Command` → Scene, Assets。
- `ToolAPI` → Command, Scene, Assets, Renderer(经 ToolContext 受控访问)。
- `Editor` → ToolAPI。
- `Domain(农场领域层)` → Engine/Scene/Assets(后期里程碑;复用引擎能力,可经 Tool API 提供编辑工具)。
- `Sandbox` → Engine(运行时直调)。

## 4. 双轨与受控约束(核心)

- **双轨制**:游戏运行时逻辑用**直接 C++ 引擎 API**(高频、性能敏感);编辑期变更、自动化测试、未来 Agent **只能通过 ToolAPI**。
- **Tool 不直接改底层对象**:涉及场景/资源修改的 Tool **必须**通过构造 `ICommand` 并经 `CommandStack` 执行,从而天然获得 Undo/Redo。只读 Tool 不走 Command。
- **ToolContext 是受控边界**:Tool 拿不到全局引擎指针,只能通过注入的 `ToolContext` 访问被显式授权的子系统接口(契合「禁 Singleton / 禁全局可变状态 / 依赖注入」)。
- **白名单 / 权限在 ToolRegistry**:未来 Agent 只能调用注册为 `agent-allowed` 的 Tool;危险或未列入的一律拒绝。
- 安全基础 = **dry-run 预览 + 参数校验 + 执行日志(可审计)+ Command 回滚**。

## 5. 各子系统内部组成与职责

### Core(基础设施,零外部依赖)
- **Math(2D)**:`Vector2`(主)、`Vector3/4`(颜色/齐次)、`Matrix4x4`(变换 + 正交投影)、`Transform2D`(position/rotation/scale + 父子)、`Rect`/`AABB`。**约定:世界空间 Y 轴向上、正交投影**,坐标系约定在数学库文档中钉死。
- **Containers / Memory**:`Handle<T>`(id + generation)、类型化池 / `SparseSet`、线性/帧分配器。
- **基础设施**:`Log`、`Assert`、`Event`、`TypeId`(编译期类型 id)。

### Platform(操作系统隔离,Win32)
- `Window`、`Input`、`Time`、`FileSystem`。平台无关接口对外,Win32 细节封死在 `.cpp` 内。

### RHI(DX12 薄封装 —— 学习重点)
- `Device`、`SwapChain`、`CommandQueue`/`CommandList`、`Fence`、`DescriptorHeap`/分配器、`GpuBuffer`、`GpuTexture`、`PipelineState`、`RootSignature`、`Shader`。
- 重点封装:**资源屏障**、**描述符堆管理**、**双/三缓冲 + Fence 同步**。裸 `ID3D12*` 不出 RHI 层。

### Assets
- `AssetManager`(句柄、缓存、引用计数、生命周期)、`Importer`(图片 → `TextureData`;图集/瓦片集描述;地图/内容 JSON)、运行时资源(经 RHI 创建)、`Sprite`/`Atlas`/`Tileset`、内容数据(物品/配方/对话从 JSON)。
- **句柄化**:上层拿 `TextureHandle`/`SpriteHandle`/`TilesetHandle` 等,非裸指针;同路径只加载一次。

### Scene(混合模型)
- `Entity`、`Scene`、`Transform2D`(父子层级 + 脏标记)。
- **Component(数据型)**:`SpriteRendererComponent`(SpriteHandle + 颜色 + 排序)、`TileMapComponent`(TilesetHandle + 瓦片网格)、`CameraComponent`(正交)。存储隐藏在 `ComponentStorage` 接口后。
- **System**:`TransformSystem`、`RenderSystem`(收集可见精灵/瓦片,按层/Y 排序产出 `RenderView`)等。

### Renderer(2D)
- `Renderer`(`BeginFrame`/`Submit`/`EndFrame`)、`SpriteBatch`(合批减少 drawcall)、`TileMapRenderer`、正交 `Camera`、排序(图层 + Y 排序实现 2.5D 遮挡)。
- 消费 Scene 产出的 `RenderView`,解析句柄,调用 RHI 录制命令,**不直接碰 DX12**。

### Command 中枢
- `ICommand`:`execute()` / `undo()` / `describe()`(供 dry-run)。
- `CommandStack`:Undo/Redo 栈。
- 具体命令:`CreateEntityCmd`、`DestroyEntityCmd`、`AddComponentCmd`、`SetTransformCmd`、`PaintTileCmd`、`SaveSceneCmd` 等;每个封装一次可回滚变更。

### ToolAPI 层
- `ToolRegistry`(注册/发现/列举/白名单裁决)、`Tool` 接口、`ToolContext`(注入的受控门面:SceneEditAPI、AssetAPI、TileMapAPI、LogQueryAPI 等)、`ToolInvocation`(可序列化调用记录)。

### Editor(ToolAPI 客户端)
- 集成 Dear ImGui:`Hierarchy`、`Inspector`、`TileMap 编辑器`、`Stats`、`Log` 面板。**所有变更经 Tool API**,只读展示直接查询。可一键关闭。

### Domain 农场领域层(后期里程碑)
- 时间系统(分钟/天/季节循环)、作物生长、库存/物品系统、NPC 日程调度。**全部数据驱动**(作物表/物品表/日程/对话从 JSON),复用引擎 Scene/Assets/Tool API,不污染核心引擎。

### Engine(应用层)
- `Application`(Init → 主循环 → Shutdown)、子系统编排、`Layer` 栈(Game 层、Editor 层)。

## 6. Tool 的统一形态与执行模型

### Tool 统一接口
```
Tool {
  name            // 唯一名,如 "scene.create_entity" / "tilemap.paint"
  category        // query | mutation
  paramsSchema    // JSON Schema:类型/必填/范围 → 用于校验
  resultSchema    // 结果结构描述
  permission      // editor-only | automation | agent-allowed (白名单)
  mutating        // 是否改场景/资源(决定是否走 Command)

  validate(json)   // 仅校验参数,不执行 → 返回 ok / 错误列表
  dryRun(ctx,json) // 校验 + 预演:产出"将会发生什么"的 diff/描述,不落地
  run(ctx,json)    // 校验 → (mutating: 构造 Command 经 CommandStack 执行) → 返回结果
}
```

### 统一执行流水线(每次调用都经过)
```
invoke(toolName, jsonParams, callerRole):
  1. 查 Registry:存在? caller 角色是否在该 Tool 白名单?   否→拒绝(PermissionDenied)
  2. validate:按 paramsSchema 校验 JSON                  失败→返回结构化错误(不执行)
  3. 若 dryRun 模式:调用 dryRun → 返回预览 diff/描述,结束(零副作用)
  4. run:mutating 工具构造 ICommand → CommandStack.execute(可 Undo)
  5. 记录 ToolInvocation:输入参数、结果、耗时、错误、产生的 Command id、diff
  6. 返回统一结果对象 { ok, result | error, invocationId }
```

### 参数与结果格式
- **JSON 为通用边界格式**(nlohmann/json):Tool 参数与结果以 JSON 传递,每个 Tool 携带 **JSON Schema** 用于自动校验。天然适配未来 LLM 工具调用,也符合「最小依赖」。

### 统一错误模型(不抛异常)
- `ToolResult = { ok:bool, code, message, data }`。错误码:`UnknownTool` / `PermissionDenied` / `InvalidParams` / `PreconditionFailed` / `ExecutionFailed`。所有失败结构化返回 + 写日志,绝不静默。

### 执行日志 / 可验证性
- 每次调用生成 `ToolInvocation`(可序列化为 JSON),进入 `Log` 与可查询历史。`ToolInvocation` + dry-run + Command 回滚 = 「能力可审计、可预览、可回滚」的 Agent-ready 安全基础。

### 首批 Tool(当前阶段范围)
- **只读(query)**:`scene.list_entities`、`scene.get_entity`、`log.read`。
- **变更(mutation,经 Command)**:`scene.create_entity`、`scene.destroy_entity`、`entity.add_component`、`entity.set_transform`、`tilemap.paint`、`scene.save`、`scene.load`。

## 7. 数据流与帧循环

```
Application::Run()  每帧:
  1. Platform   → 泵消息、采集 Input、计算 deltaTime
  2. 编辑期变更 → Editor/Automation 经 ToolAPI.invoke(...) → 变更型 Tool 构造 ICommand → CommandStack.execute()
  3. Game Layer → 运行时逻辑:直调 C++ API 改 Entity/组件(高频路径)
  4. Scene      → TransformSystem 刷新世界矩阵 → RenderSystem 生成 RenderView(精灵/瓦片,按层+Y 排序)
  5. Renderer   → BeginFrame → SpriteBatch 合批录制 →(Editor 录制 ImGui)→ EndFrame/Present/Fence
  6. Assets     → 处理延迟卸载/(后期)异步加载完成回调
```

**关键流向约定:**
- 编辑期变更经 ToolAPI→Command,运行时逻辑直调引擎 API,分轨。
- Scene → Renderer 单向传递 `RenderView` 纯数据,Scene 不持有 RHI 资源,可独立测试。
- 句柄解析点集中在 Renderer/RHI 边界。GPU/CPU 同步封装在 RHI 内。

## 8. 目录结构

```
MiniEngine/
├─ CMakeLists.txt
├─ engine/
│  ├─ core/  platform/  rhi/  assets/  scene/  renderer/
│  ├─ command/        # ICommand / CommandStack / 具体命令
│  ├─ toolapi/        # ToolRegistry / Tool / ToolContext / ToolInvocation / 内置 Tool
│  ├─ editor/         # ImGui 面板(ToolAPI 客户端,含瓦片地图编辑)
│  ├─ domain/         # 农场领域层(时间/瓦片玩法/作物/物品/NPC 日程,后期)
│  └─ engine/         # Application / Layer / 子系统编排
│  └─ 每个模块: include/ 与 src/,自成一个静态库 target
├─ sandbox/           # 农场 demo(可执行,运行时直调)
├─ tests/             # 单元测试
├─ assets/            # 运行时资源(图集/瓦片集/着色器 + 内容 JSON:物品/配方/对话/地图)
├─ third_party/       # 第三方依赖
└─ docs/              # 开发文档(本设计 + PROGRESS + 各子系统文档)
```

## 9. 构建系统

- **CMake**(`>= 3.20`)。每模块一个 `STATIC` 库 target,按分层显式声明依赖,**由构建图强制单向依赖**。
- 生成 Visual Studio 解决方案(`-G "Visual Studio 17 2022"`),Debug/Release。
- 着色器 `.hlsl` 用 DXC(`dxcompiler`)**运行时编译**。

## 10. 第三方依赖(尽量少)

| 依赖 | 用途 |
|------|------|
| nlohmann/json | Tool 参数/结果、场景与内容(物品/配方/对话/地图)序列化 |
| Dear ImGui | 调试 GUI(ToolAPI 客户端) |
| stb_image | 贴图/图集加载 |
| DirectX-Headers / D3D12 Agility SDK | DX12 头与运行时 |
| doctest 或 Catch2 | 单元测试 |

全部放 `third_party/`,优先 CMake `FetchContent` 或 git submodule。**不使用 SDL2、不使用 EnTT**。JSON Schema 校验优先用轻量库或基于 nlohmann/json 手写,避免再引入重依赖。瓦片地图可选兼容 Tiled 的 JSON 导出格式。

## 11. 代码约定

- 命名:`PascalCase` 类型/函数,`m_camelCase` 成员,`s_` 静态,`g_` 全局;头文件 `#pragma once`。
- 命名空间:`me::`(再细分 `me::rhi`、`me::scene`、`me::cmd`、`me::tool`、`me::farm` 等)。
- **错误处理**:可恢复 → 返回值 / `Result` / `std::optional`;不变量违反 → `ME_ASSERT`;DX12 `HRESULT` → `ME_HR_CHECK`;Tool 失败 → `ToolResult`。**不使用 C++ 异常**。
- **资源所有权**:句柄归 `AssetManager`;RHI 资源 RAII;裸 `ID3D12*` 不出 RHI 层。
- **受控访问**:Tool 仅经 `ToolContext` 访问引擎,禁止持有全局指针或绕过 Command 改场景/资源。
- **数据驱动**:游戏数值/内容(物品/配方/对话/地图/作物表)从 JSON 读取,源码零硬编码内容字符串与魔法数字。

## 12. 测试策略

- **Core 必单测**:Math(2D 变换/正交投影/Rect)、Handle、容器、分配器(纯 CPU,doctest)。
- **Scene 可单测**:Transform 层级、组件增删、System 产出 RenderView(不碰 GPU)。
- **Command 必单测**:每个命令 execute/undo 往返一致性。
- **ToolAPI 必单测**:参数校验(合法/非法)、白名单裁决、dry-run 零副作用、错误码、ToolInvocation 记录完整性。
- **Domain 可单测**:时间推进、作物生长状态机、库存逻辑等纯逻辑(不依赖 GPU)。
- **RHI / Renderer**:不强求自动化,靠 sandbox + ImGui 目视验证(画面、帧率、drawcall)。

## 13. 里程碑路线图

每个里程碑 = 一个可独立 `spec → plan → 实现` 的子项目。**当前阶段交付止于 M7(接口层 + 编辑器),不接大模型**;农场领域层 M8 起。

| 里程碑 | 内容 | 学习重点 |
|--------|------|----------|
| **M0 地基** | CMake 骨架 + Core(2D Math/Log/Handle)+ Platform(窗口/输入/计时)+ 单测 | 工程骨架、2D 数学库 |
| **M1 精灵上屏** | RHI 最小可用(Device/SwapChain/CmdList/Fence/PSO)→ 清屏 → 画一个带纹理精灵 | **DX12 同步与命令录制** |
| **M2 批渲染 + 正交相机** | SpriteBatch 合批 + 正交相机 + 多精灵 | 2D 渲染管线 |
| **M3 瓦片地图** | Tileset + TileMap 渲染 + 从 JSON 加载地图 | 数据驱动地图 |
| **M4 Scene + 组件** | Entity/Transform2D + Component + System,RenderSystem 收集提交 | 场景模型、System |
| **M5 Command 中枢** | `ICommand` + `CommandStack` + Undo/Redo,先覆盖创建实体/改 Transform/绘制瓦片 | 可回滚变更模型 |
| **M6 ToolAPI** | `ToolRegistry` + Tool 接口 + JSON Schema 校验 + dry-run + 执行日志 + 首批 Tool + 权限白名单 | **受控、可验证的能力接口** |
| **M7 Editor as Client** | ImGui 面板(含瓦片地图编辑)改为**通过 Tool API**操作 | 接口完备性反向验证 |
| **M8 农场领域层** | 时间系统(天/季节)+ 作物生长 + 库存/物品(JSON)+ NPC 日程调度 | 领域玩法、数据驱动 |
| **M9+(可选 / 未来)** | 对话/配方数据驱动、2D 物理/碰撞、存档;**未来:Agent 接入白名单 Tool、LLM 工具调用** | 进阶子系统 / Agent 接入 |

## 14. 下一步

按里程碑顺序,从 **M0** 开始,各里程碑各自走 `writing-plans → 实现` 流程。ToolAPI 主线(M5 → M6 → M7)是本项目区别于普通学习引擎的核心特色;农场领域玩法(M8)在引擎与 Tool API 就绪后展开。
