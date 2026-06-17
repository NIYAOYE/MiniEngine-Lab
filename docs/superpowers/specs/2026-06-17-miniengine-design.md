# MiniEngine 架构设计文档 —— Agent-ready Engine API

- **日期**:2026-06-17(2026-06-17 调整方向:Agent-ready Engine API)
- **状态**:已与用户确认
- **类型**:学习型 3D 游戏引擎,面向「可被工具/自动化/未来 Agent 受控调用」的 API 设计

## 1. 项目目标与约束

MiniEngine 是一个从零开始、以**学习和掌握游戏引擎原理**为首要目标的 3D 游戏引擎。在引擎底座之上,本项目额外确立一个方向:**将引擎编辑能力抽象为统一、受控、可验证的 Tool API**,使编辑器、脚本、自动化测试与未来的 Editor Agent 都能通过同一套接口调用引擎能力。

**当前阶段明确不实现完整 Agent、不接入大模型**,只建设接口层本身(Tool Registry、Command/Undo、参数校验、执行日志、错误返回、dry-run 预览),并以引擎自身的编辑器作为第一个客户端来反向验证接口完备性。

| 维度 | 决策 |
|------|------|
| 引擎类型 | 完整游戏引擎(渲染 + 场景/实体 + 资源 + 输入 + 后期物理) |
| 语言 | C++ |
| 图形 API | DirectX 12 |
| 平台 | Windows |
| 维度 | 3D 为主 |
| 游戏逻辑 | 运行时纯 C++ 代码直调;编辑期变更走 Tool API + Dear ImGui 调试 GUI |
| 首要原则 | **代码清晰可读 > 极致性能**;接口稳定、模块解耦、可独立理解与测试 |
| 受控原则 | 编辑期对场景/资源的修改必须**可校验、可预览、可审计、可回滚** |

**非目标(YAGNI)**:当前阶段不实现完整 Agent、不接入 LLM、不做可视化关卡编辑器、不做脚本语言绑定、不追求生产级性能优化。这些不在当前范围内。

## 2. 实体/场景模型决策

采用**混合模型(方案 C)**:OOP 场景层级 + 轻量数据型组件 + System 处理。

- 以 `Entity`(含 Transform 父子层级)为骨架,概念贴近 Unity,直观好学。
- 组件尽量是**纯数据 + 极少逻辑**;更新逻辑放在 System 中,由 System 遍历组件处理(而非组件自更新),养成数据导向习惯。
- 组件存储隐藏在 `ComponentStorage` 接口之后,**后期可演进为纯 ECS 存储而不破坏上层**。

**为何不选纯 ECS**:纯 ECS(archetype/查询/原型迁移)学习曲线陡,容易让精力耗在框架实现本身而非引擎其他子系统。混合模型在「直观易懂」与「接触现代数据导向思想」之间取得平衡,契合学习目标。

## 3. 分层架构

核心原则:**自下而上单向依赖**,上层依赖下层,下层绝不反向依赖上层。每层职责单一、可独立理解和测试。依赖方向由 CMake 构建图强制。

在引擎底座之上新增 **Command 中枢** 与 **ToolAPI 层**:它们不替换直接的 C++ 引擎 API,而是为「编辑期/自动化/未来 Agent」提供唯一的受控变更入口。

```
┌──────────────────────────────────────────────────────────────┐
│  Sandbox / Game        运行时:直接调用 C++ 引擎 API(高频/性能)   │
├──────────────────────────────────────────────────────────────┤
│  Editor / Automation / (未来)Agent   ← 这些客户端只通过下面的     │
│                                         Tool API 做编辑期变更     │
├──────────────────────────────────────────────────────────────┤
│  ToolAPI 层 (新增)                                              │
│   ├─ ToolRegistry      统一注册/发现/白名单/权限                  │
│   ├─ Tool (接口)        name + JSON Schema + validate/dryRun/run │
│   ├─ ToolContext        受控访问引擎子系统的唯一入口(注入)         │
│   └─ ToolInvocation     执行记录:输入/结果/错误/diff/日志         │
├──────────────────────────────────────────────────────────────┤
│  Command 中枢 (新增)                                            │
│   ├─ ICommand          execute / undo / (描述用于 dry-run)       │
│   ├─ CommandStack       Undo/Redo 栈                            │
│   └─ 具体命令           CreateEntityCmd / AddComponentCmd / ...   │
├──────────────────────────────────────────────────────────────┤
│  Engine (应用层)        主循环、子系统编排、Application            │
├──────────────┬───────────────┬──────────────┬─────────────────┤
│  Scene        │  Renderer     │  Physics      │  (Editor 已上移   │
│  (实体/组件/    │  (渲染图/材质/  │  (后期里程碑)   │   为 ToolAPI 客户端)│
│   System)     │   相机/光照)    │               │                 │
├──────────────┴───────────────┴──────────────┴─────────────────┤
│  Assets         资源加载/缓存/生命周期(Mesh/Texture/Shader/      │
│                 Material),句柄化访问,导入器(模型/图片)            │
├──────────────────────────────────────────────────────────────┤
│  RHI            DX12 薄封装(Device/CmdList/Buffer/Texture/      │
│                 PSO/Descriptor/Fence 同步)                       │
├──────────────────────────────────────────────────────────────┤
│  Platform       窗口、输入、计时、文件、动态库(Win32)              │
├──────────────────────────────────────────────────────────────┤
│  Core           数学库、容器、日志、断言、内存、事件、句柄、类型id    │
└──────────────────────────────────────────────────────────────┘
```

依赖边(单向):
- `Core` ← 零外部依赖。
- `Platform` → Core。
- `RHI` → Core, Platform。
- `Assets` → Core, Platform, RHI(加载时调用 RHI 创建 GPU 资源)。
- `Scene` → Core, Assets(持有句柄,不持有 GPU 资源)。
- `Renderer` → Core, RHI, Assets。
- `Engine` → Scene, Renderer, Assets, RHI, Platform, Core,负责编排。
- `Command` → Scene, Assets(具体命令操作场景/资源)。
- `ToolAPI` → Command, Scene, Assets, Renderer(经 ToolContext 受控访问)。
- `Editor` → ToolAPI(作为客户端;不再被引擎其它层依赖,可一键关闭)。
- `Sandbox` → Engine(运行时直调)。

## 4. 双轨与受控约束(核心)

- **双轨制**:游戏运行时逻辑用**直接 C++ 引擎 API**(高频、性能敏感);编辑期变更、自动化测试、未来 Agent **只能通过 ToolAPI**。
- **Tool 不直接改底层对象**:所有涉及场景/资源修改的 Tool,**必须**通过构造 `ICommand` 并经 `CommandStack` 执行,从而天然获得 Undo/Redo。只读 Tool(读日志、查询实体)不走 Command。
- **ToolContext 是受控边界**:Tool 拿不到全局引擎指针,只能通过注入的 `ToolContext` 访问被显式授权的子系统接口(契合「禁 Singleton / 禁全局可变状态 / 依赖注入」约束)。
- **白名单 / 权限在 ToolRegistry**:未来 Agent 只能调用注册为 `agent-allowed` 的 Tool;危险或未列入的一律拒绝。
- 整套能力的安全基础 = **dry-run 预览 + 参数校验 + 执行日志(可审计)+ Command 回滚**。

## 5. 各子系统内部组成与职责

### Core(基础设施,零外部依赖)
- **Math**:`Vector2/3/4`、`Matrix4x4`、`Quaternion`、`Transform`(SRT)、AABB、射线。**约定:DX12 左手坐标系,行主序 + 行向量**(与 HLSL 默认一致),此约定在数学库文档中钉死。
- **Containers / Memory**:`Handle<T>`(id + generation,核心抽象)、类型化池 / `SparseSet`、线性/帧分配器。
- **基础设施**:`Log`、`Assert`、`Event`、`TypeId`(编译期类型 id)。

### Platform(操作系统隔离,Win32)
- `Window`、`Input`、`Time`、`FileSystem`。对外只暴露平台无关接口,Win32 细节封死在 `.cpp` 内。

### RHI(DX12 薄封装 —— 学习重点)
- `Device`、`SwapChain`、`CommandQueue`/`CommandList`、`Fence`、`DescriptorHeap`/分配器、`GpuBuffer`、`GpuTexture`、`PipelineState`、`RootSignature`、`Shader`。
- 重点封装:**资源屏障(状态转换)**、**描述符堆管理**、**帧间双/三缓冲与 Fence 同步**。裸 `ID3D12*` 不得出 RHI 层。

### Assets
- `AssetManager`(句柄分配、缓存、引用计数、生命周期)、`Importer`(glTF/OBJ、图片)、运行时资源(经 RHI 创建)、`Material`。
- **句柄化**:上层拿 `MeshHandle`/`TextureHandle`/`MaterialHandle`(id + 版本号),非裸指针。同路径只加载一次。

### Scene(混合模型)
- `Entity`、`Scene`、`Transform`(父子层级 + 脏标记)。
- **Component(数据型)**:`MeshRendererComponent`、`CameraComponent`、`LightComponent`,存储隐藏在 `ComponentStorage` 接口后。
- **System**:`TransformSystem`、`RenderSystem` 等,遍历组件产出数据。

### Renderer
- `Renderer`(`BeginFrame`/`Submit`/`EndFrame`)、初期固定 ForwardPass、`Camera`、`Light`、`MaterialBinding`。
- 消费 Scene 产出的 `RenderView`,解析句柄,调用 RHI 录制命令,**不直接碰 DX12**。

### Command 中枢(新增)
- `ICommand`:`execute()` / `undo()` / `describe()`(供 dry-run 产出"将会发生什么")。
- `CommandStack`:Undo/Redo 栈,记录已执行命令。
- 具体命令:`CreateEntityCmd`、`DestroyEntityCmd`、`AddComponentCmd`、`SetTransformCmd`、`SaveSceneCmd` 等;每个命令封装一次可回滚的场景/资源变更。
- 任何变更型 Tool 都落到某个 `ICommand`,这是「编辑期变更可回滚」的唯一实现路径。

### ToolAPI 层(新增)
- `ToolRegistry`:注册、按名发现、列出可用 Tool、按调用者角色做白名单/权限裁决。
- `Tool` 接口:统一形态(见第 6 节)。
- `ToolContext`:注入给 Tool 的受控门面,暴露被授权的子系统接口(如 SceneEditAPI、AssetAPI、LogQueryAPI),Tool 不得绕过它触达底层对象。
- `ToolInvocation`:单次调用的可序列化记录(输入参数、结果、错误、耗时、产生的 Command id、diff/描述),进入日志与可查询历史。

### Editor(ToolAPI 客户端)
- 集成 Dear ImGui:`Hierarchy`、`Inspector`、`Stats`、`Log` 面板。
- **所有变更操作通过 Tool API 发起**(吃自己的狗粮,验证接口完备),只读展示可直接查询。可一键关闭。

### Engine(应用层)
- `Application`(Init → 主循环 → Shutdown)、子系统注册与更新顺序编排、`Layer` 栈(Game 层、Editor 层)。

## 6. Tool 的统一形态与执行模型

### Tool 统一接口
每个 Tool 由 `ToolRegistry` 注册,具备:

```
Tool {
  name            // 唯一名,如 "scene.create_entity"
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
- **JSON 为通用边界格式**(nlohmann/json):Tool 参数与结果以 JSON 传递,每个 Tool 携带 **JSON Schema** 描述用于自动校验。
- 该选择天然适配未来 LLM 工具调用(tool-use 的入参/出参即 JSON),也符合「最小依赖、优先 nlohmann/json」。

### 统一错误模型(不抛异常,符合引擎约定)
- `ToolResult = { ok:bool, code, message, data }`。
- 错误码枚举:`UnknownTool` / `PermissionDenied` / `InvalidParams` / `PreconditionFailed` / `ExecutionFailed`。
- 所有失败结构化返回 + 写日志,绝不静默忽略。

### 执行日志 / 可验证性
- 每次调用生成 `ToolInvocation` 记录(可序列化为 JSON),进入引擎 `Log` 与可查询的 invocation 历史。
- `ToolInvocation` 记录 + dry-run 预览 + Command 回滚,共同构成「能力可审计、可预览、可回滚」的 Agent-ready 安全基础。

### 首批 Tool(当前阶段范围,验证整套机制即可)
- **只读(query)**:`scene.list_entities`、`scene.get_entity`、`log.read`。
- **变更(mutation,经 Command)**:`scene.create_entity`、`scene.destroy_entity`、`entity.add_component`、`entity.set_transform`、`scene.save`、`scene.load`。

## 7. 数据流与帧循环

```
Application::Run()  每帧:
  1. Platform   → 泵消息、采集 Input、计算 deltaTime
  2. 编辑期变更 → Editor/Automation 通过 ToolAPI.invoke(...) 发起变更
                  → 变更型 Tool 构造 ICommand → CommandStack.execute()(可 Undo)
  3. Game Layer → 运行时游戏逻辑:直调 C++ API 改 Entity/组件(高频路径)
  4. Scene      → TransformSystem 刷新世界矩阵(脏标记驱动)
                → RenderSystem 生成 RenderView(draw item 列表)
  5. Renderer   → BeginFrame → 录制 draw 命令 →(Editor 录制 ImGui)→ EndFrame/Present/Fence
  6. Assets     → 处理延迟卸载/(后期)异步加载完成回调
```

**关键流向约定:**
- **编辑期变更与运行时逻辑分轨**:前者经 ToolAPI→Command,后者直调引擎 API。
- **Scene → Renderer 单向数据传递**:Scene 只产出 `RenderView` 纯数据,不持有 RHI 资源,可独立测试。
- **句柄解析点集中在 Renderer/RHI 边界**,避免裸指针扩散。
- **GPU/CPU 同步封装在 RHI 内**,上层只感知 `BeginFrame`/`EndFrame`。

## 8. 目录结构

```
MiniEngine/
├─ CMakeLists.txt
├─ engine/
│  ├─ core/  platform/  rhi/  assets/  scene/  renderer/
│  ├─ command/        # ICommand / CommandStack / 具体命令
│  ├─ toolapi/        # ToolRegistry / Tool / ToolContext / ToolInvocation / 内置 Tool
│  ├─ editor/         # ImGui 面板(ToolAPI 客户端)
│  └─ engine/         # Application / Layer / 子系统编排
│  └─ 每个模块: include/ 与 src/,自成一个静态库 target
├─ sandbox/           # 用引擎写的 demo(可执行,运行时直调)
├─ tests/             # 单元测试
├─ assets/            # 运行时资源(模型/贴图/着色器 .hlsl)
├─ third_party/       # 第三方依赖
└─ docs/              # 开发文档(本设计 + 各子系统文档)
```

## 9. 构建系统

- **CMake**(`>= 3.20`)。每个模块一个 `STATIC` 库 target,按分层用 `target_link_libraries` 显式声明依赖,**由构建图强制单向依赖**。
- 生成 Visual Studio 解决方案(`-G "Visual Studio 17 2022"`),配置 Debug/Release。
- 着色器 `.hlsl` 用 DXC(`dxcompiler`)**运行时编译**(学习期最简单,免离线编译步骤)。

## 10. 第三方依赖(尽量少)

| 依赖 | 用途 |
|------|------|
| nlohmann/json | Tool 参数/结果与场景序列化(JSON 边界格式) |
| Dear ImGui | 调试 GUI(ToolAPI 客户端) |
| assimp 或 cgltf | 模型导入 |
| stb_image | 贴图加载 |
| DirectX-Headers / D3D12 Agility SDK | DX12 头与运行时 |
| doctest 或 Catch2 | 单元测试 |

全部放 `third_party/`,优先用 CMake `FetchContent` 或 git submodule 管理。JSON Schema 校验优先用轻量库或基于 nlohmann/json 手写校验,避免再引入重依赖。

## 11. 代码约定

- 命名:`PascalCase` 类型/函数,`m_camelCase` 成员,`s_` 静态,`g_` 全局;头文件用 `#pragma once`。
- 命名空间:`me::`(模块再细分,如 `me::rhi`、`me::scene`、`me::cmd`、`me::tool`)。
- **错误处理**:可恢复错误 → 返回值 / `Result` / `std::optional`;程序员错误/不变量违反 → `ME_ASSERT`;DX12 `HRESULT` 失败 → `ME_HR_CHECK`;Tool 调用失败 → `ToolResult` 结构化错误。**不使用 C++ 异常**。
- **资源所有权**:句柄归 `AssetManager`;RHI 资源用 RAII 包装;裸 `ID3D12*` 不出 RHI 层。
- **受控访问**:Tool 仅经 `ToolContext` 访问引擎,禁止持有全局引擎指针或绕过 Command 直接改场景/资源。

## 12. 测试策略

配合学习目标,在能纯逻辑验证处写测试,GPU 部分靠可视化验证。

- **Core 必单测**:Math、Handle、容器、分配器(纯 CPU、确定性,doctest)。
- **Scene 可单测**:Transform 层级世界矩阵、组件增删、System 产出 RenderView(不碰 GPU)。
- **Command 必单测**:每个命令的 execute/undo 往返一致性(执行后 undo 应还原状态)。
- **ToolAPI 必单测**:参数校验(合法/非法 JSON)、权限白名单裁决、dry-run 零副作用、错误码正确性、ToolInvocation 记录完整性。这一层逻辑性强、不依赖 GPU,是自动化测试的重点。
- **RHI / Renderer**:不强求自动化,靠 sandbox + ImGui 目视验证(画面、帧率、drawcall)。

## 13. 里程碑路线图

每个里程碑 = 一个可独立 `spec → plan → 实现` 的子项目。**当前阶段到 M5 为止:只建接口层,不接大模型。**

| 里程碑 | 内容 | 学习重点 |
|--------|------|----------|
| **M0 地基** | CMake 骨架 + Core(Math/Log/Handle)+ Platform(窗口/输入/计时)+ 单测 | 工程骨架、数学库 |
| **M1 三角形上屏** | RHI 最小可用(Device/SwapChain/CmdList/Fence/PSO)→ 清屏 → 画三角形 | **DX12 同步与命令录制** |
| **M2 网格与相机** | Assets(导入 OBJ/glTF + 贴图)+ Renderer ForwardPass + Camera + 纹理模型旋转 | 资源管线、相机变换 |
| **M3 Scene + 多对象** | Entity/Transform 层级 + Component + System,多对象,RenderSystem 收集提交 | 场景模型、System |
| **M3.5 Command 中枢** | `ICommand` + `CommandStack` + Undo/Redo,先覆盖创建实体/改 Transform | 可回滚变更模型 |
| **M4 ToolAPI** | `ToolRegistry` + Tool 接口 + JSON Schema 校验 + dry-run + 执行日志 + 首批 Tool + 权限白名单 | **受控、可验证的能力接口** |
| **M5 Editor as Client** | ImGui 面板改为**通过 Tool API**操作(吃自己的狗粮,验证接口完备) | 接口完备性反向验证 |
| **M6 光照与材质** | 基础 Blinn-Phong 或简化 PBR + 多光源 + 材质系统 | 光照、材质 |
| **M7+(可选 / 未来)** | Physics、阴影、RenderGraph、异步资源/热重载、向 ECS 演进;**未来:Agent 接入白名单 Tool、LLM 工具调用** | 进阶子系统 / Agent 接入 |

## 14. 下一步

按里程碑顺序,从 **M0** 开始,各里程碑各自走 `writing-plans → 实现` 流程。ToolAPI 主线(M3.5 → M4 → M5)是本项目区别于普通学习引擎的核心特色,需在 Scene(M3)就绪后重点推进。
