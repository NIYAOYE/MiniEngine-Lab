# MiniEngine 架构设计文档

- **日期**:2026-06-17
- **状态**:已与用户确认
- **类型**:学习型 3D 游戏引擎

## 1. 项目目标与约束

MiniEngine 是一个从零开始、以**学习和掌握游戏引擎原理**为首要目标的 3D 游戏引擎。

| 维度 | 决策 |
|------|------|
| 引擎类型 | 完整游戏引擎(渲染 + 场景/实体 + 资源 + 输入 + 后期物理) |
| 语言 | C++ |
| 图形 API | DirectX 12 |
| 平台 | Windows |
| 维度 | 3D 为主 |
| 游戏逻辑 | 纯 C++ 代码驱动 + Dear ImGui 调试 GUI(无独立编辑器/脚本系统) |
| 首要原则 | **代码清晰可读 > 极致性能**;接口稳定、模块解耦、可独立理解与测试 |

**非目标(YAGNI)**:不做完整可视化关卡编辑器、不做脚本语言绑定、不追求生产级性能优化。这些不在当前范围内。

## 2. 实体/场景模型决策

采用**混合模型(方案 C)**:OOP 场景层级 + 轻量数据型组件 + System 处理。

- 以 `Entity`(含 Transform 父子层级)为骨架,概念贴近 Unity,直观好学。
- 组件尽量是**纯数据 + 极少逻辑**;更新逻辑放在 System 中,由 System 遍历组件处理(而非组件自更新),养成数据导向习惯。
- 组件存储隐藏在 `ComponentStorage` 接口之后,**后期可演进为纯 ECS 存储而不破坏上层**。

**为何不选纯 ECS**:纯 ECS(archetype/查询/原型迁移)学习曲线陡,容易让精力耗在框架实现本身而非引擎其他子系统。混合模型在"直观易懂"与"接触现代数据导向思想"之间取得平衡,契合学习目标。

## 3. 分层架构

核心原则:**自下而上单向依赖**,上层依赖下层,下层绝不反向依赖上层。每层职责单一、可独立理解和测试。依赖方向由 CMake 构建图强制。

```
┌─────────────────────────────────────────────────────────┐
│  Sandbox / Game        游戏 demo(用引擎写的可执行示例)         │
├─────────────────────────────────────────────────────────┤
│  Engine (应用层)        主循环、子系统编排、Application          │
├──────────────┬───────────────┬──────────────┬────────────┤
│  Scene        │  Renderer     │  Physics      │  Editor    │
│  (实体/组件/    │  (渲染图/材质/  │  (后期里程碑)   │  (ImGui)   │
│   System)     │   相机/光照)    │               │            │
├──────────────┴───────────────┴──────────────┴────────────┤
│  Assets         资源加载/缓存/生命周期(Mesh/Texture/Shader/   │
│                 Material),句柄化访问,导入器(模型/图片)         │
├─────────────────────────────────────────────────────────┤
│  RHI            DX12 薄封装(Device/CmdList/Buffer/Texture/  │
│                 PSO/Descriptor/Fence 同步)                   │
├─────────────────────────────────────────────────────────┤
│  Platform       窗口、输入、计时、文件、动态库(Win32)            │
├─────────────────────────────────────────────────────────┤
│  Core           数学库、容器、日志、断言、内存、事件、句柄、类型id  │
└─────────────────────────────────────────────────────────┘
```

依赖边(单向):
- `Core` ← 零外部依赖。
- `Platform` → Core。
- `RHI` → Core, Platform。
- `Assets` → Core, Platform, RHI(加载时调用 RHI 创建 GPU 资源)。
- `Scene` → Core, Assets(持有句柄,不持有 GPU 资源)。
- `Renderer` → Core, RHI, Assets。
- `Editor` → Core, Scene, Renderer(通过受控/只读接口观察,可一键关闭;不被它们依赖)。
- `Engine` → 以上全部,负责编排。
- `Sandbox` → Engine。

## 4. 各子系统内部组成与职责

### Core(基础设施,零外部依赖)
- **Math**:`Vector2/3/4`、`Matrix4x4`、`Quaternion`、`Transform`(SRT)、AABB、射线。**约定:DX12 左手坐标系,行主序 + 行向量**(与 HLSL 默认一致),此约定在数学库文档中钉死。
- **Containers / Memory**:`Handle<T>`(id + generation,核心抽象)、类型化池 / `SparseSet`(为方案 C 的组件存储铺路)、线性/帧分配器。
- **基础设施**:`Log`、`Assert`、`Event`(简单观察者/分发)、`TypeId`(编译期类型 id,供组件/资源类型识别)。

### Platform(操作系统隔离,Win32)
- `Window`(创建窗口、消息泵)、`Input`(键鼠状态)、`Time`(高精度计时、帧 delta)、`FileSystem`(读文件、路径)。
- 对外只暴露平台无关接口;Win32 细节封死在 `.cpp` 内,便于将来移植。

### RHI(DX12 薄封装 —— 学习重点)
- `Device`、`SwapChain`、`CommandQueue` / `CommandList`、`Fence`(GPU/CPU 同步)、`DescriptorHeap` / 分配器、`GpuBuffer`、`GpuTexture`、`PipelineState`、`RootSignature`、`Shader`(编译 HLSL)。
- 重点封装最容易踩坑的部分:**资源屏障(状态转换)**、**描述符堆管理**、**帧间双/三缓冲与 Fence 同步**。
- 接口面向"资源 + 命令录制",隐藏裸 `ID3D12*`。裸 `ID3D12*` 不得出 RHI 层。

### Assets
- `AssetManager`(句柄分配、缓存、引用计数、生命周期)。
- `Importer`(glTF/OBJ → `MeshData`,图片 → `TextureData`)。
- 运行时资源:加载时调用 RHI 创建 GPU buffer/texture。
- `Material`(着色器 + 参数 + 贴图句柄)。
- **句柄化**:上层拿到 `MeshHandle`/`TextureHandle`/`MaterialHandle`(轻量 id + 版本号),不是裸指针。
- **缓存去重**:同一路径只加载一次,按句柄复用。
- 接口先做同步加载,后续可加异步/热重载而不破坏上层。

### Scene(混合模型)
- `Entity`(id + 名字 + Transform 节点)、`Scene`(实体集合 + 层级根)、`Transform`(父子层级,本地/世界矩阵脏标记)。
- **Component(数据型)**:`MeshRendererComponent`(MeshHandle + MaterialHandle)、`CameraComponent`、`LightComponent`。存储隐藏在 `ComponentStorage` 接口后(预留 ECS 演进)。
- **System**:`TransformSystem`(刷新世界矩阵)、`RenderSystem`(收集可见 MeshRenderer 结合 Camera/Light 产出 RenderView)等。遍历组件而非组件自更新。

### Renderer
- `Renderer`(高层接口:`BeginFrame` / `Submit` / `EndFrame`)。
- 初期为固定 ForwardPass;后期可演进为有序 Pass 列表 / RenderGraph。
- `Camera`(view/proj)、`Light`、`MaterialBinding`。
- 消费 Scene 产出的 `RenderView`(draw item),向 Assets 解析句柄拿 GPU 资源,调用 RHI 录制命令。**不直接碰 DX12**。

### Editor(叠加层)
- 集成 Dear ImGui:`Hierarchy`(实体树)、`Inspector`(选中实体看/改组件)、`Stats`(帧率/drawcall)、`Log` 面板。
- 通过受控接口访问 Scene/Renderer,**可一键关闭**。

### Engine(应用层)
- `Application`(生命周期:Init → 主循环 → Shutdown)、子系统注册与更新顺序编排、`Layer` 栈(Game 层、Editor 层)。

## 5. 数据流与帧循环

```
Application::Run()  每帧:
  1. Platform   → 泵消息、采集 Input、计算 deltaTime
  2. Game Layer → 用户游戏逻辑:改 Entity 的 Transform / 增删组件
  3. Scene      → TransformSystem 刷新世界矩阵(脏标记驱动)
                → RenderSystem 遍历 MeshRendererComponent,
                  结合 Camera/Light,生成 RenderView(draw item 列表)
  4. Renderer   → BeginFrame:RHI 取下一帧 backbuffer、重置 CommandList
                → 遍历 RenderView,绑定 PSO/材质/资源,录制 draw 命令
                → (Editor Layer 在此后录制 ImGui 绘制命令)
                → EndFrame:提交 CommandQueue、Present、Fence 同步
  5. Assets     → 处理延迟卸载/(后期)异步加载完成回调
```

**关键流向约定:**

- **Scene → Renderer 单向数据传递**:Scene 不持有 RHI 资源,只产出 `RenderView`(纯数据:变换矩阵 + MeshHandle + MaterialHandle + 相机/光照参数)。这层解耦让 Scene 可独立测试。
- **句柄解析点集中在 Renderer/RHI 边界**:上层全程只传 Handle,直到录制命令时才解析成 GPU 资源,避免裸指针扩散。
- **GPU/CPU 同步封装在 RHI 内**:双/三缓冲 + per-frame Fence,上层只感知 `BeginFrame`/`EndFrame`。
- **Editor 录制在主场景之后、Present 之前**,叠加在同一 backbuffer 上。

## 6. 目录结构

```
MiniEngine/
├─ CMakeLists.txt
├─ engine/
│  ├─ core/  platform/  rhi/  assets/  scene/  renderer/  editor/  engine/
│  └─ 每个模块: include/ 与 src/,自成一个静态库 target
├─ sandbox/          # 用引擎写的 demo(可执行)
├─ tests/            # 单元测试
├─ assets/           # 运行时资源(模型/贴图/着色器 .hlsl)
├─ third_party/      # 第三方依赖
└─ docs/             # 开发文档(本设计 + 各子系统文档)
```

## 7. 构建系统

- **CMake**(`>= 3.20`)。每个模块一个 `STATIC` 库 target,按分层用 `target_link_libraries` 显式声明依赖,**由构建图强制单向依赖**。
- 生成 Visual Studio 解决方案(`-G "Visual Studio 17 2022"`),配置 Debug/Release。
- 着色器 `.hlsl` 用 DXC(`dxcompiler`)**运行时编译**(学习期最简单,免离线编译步骤)。

## 8. 第三方依赖(尽量少)

| 依赖 | 用途 |
|------|------|
| Dear ImGui | 调试 GUI |
| assimp 或 cgltf | 模型导入 |
| stb_image | 贴图加载 |
| DirectX-Headers / D3D12 Agility SDK | DX12 头与运行时 |
| doctest 或 Catch2 | 单元测试 |

全部放 `third_party/`,优先用 CMake `FetchContent` 或 git submodule 管理。

## 9. 代码约定

- 命名:`PascalCase` 类型/函数,`m_camelCase` 成员,`s_` 静态,`g_` 全局;头文件用 `#pragma once`。
- 命名空间:`me::`(模块再细分,如 `me::rhi`、`me::scene`)。
- **错误处理**:
  - 可恢复错误 → 返回值 / `Result` / `std::optional`。
  - 程序员错误 / 不变量违反 → `ME_ASSERT`。
  - DX12 `HRESULT` 失败 → `ME_HR_CHECK` 包裹(日志 + 断言)。
  - **不使用 C++ 异常**(渲染引擎惯例)。
- **资源所有权**:句柄归 `AssetManager`;RHI 资源用 RAII 包装;裸 `ID3D12*` 不出 RHI 层。

## 10. 测试策略

配合学习目标,在能纯逻辑验证处写测试,GPU 部分靠可视化验证。

- **Core 必单测**:Math(矩阵/四元数/变换)、Handle、容器、分配器 —— 纯 CPU、确定性,用 doctest。
- **Scene 可单测**:Transform 层级世界矩阵计算、组件增删、System 遍历产出 RenderView(不碰 GPU)。
- **RHI / Renderer**:不强求自动化测试,靠 sandbox + ImGui 目视验证(画面、帧率、drawcall)。

## 11. 里程碑路线图

每个里程碑 = 一个可独立 `spec → plan → 实现` 的子项目。

| 里程碑 | 内容 | 学习重点 |
|--------|------|----------|
| **M0 地基** | CMake 骨架 + Core(Math/Log/Handle)+ Platform(窗口/输入/计时)+ 单测 | 工程骨架、数学库 |
| **M1 三角形上屏** | RHI 最小可用(Device/SwapChain/CmdList/Fence/PSO)→ 清屏 → 画三角形 | **DX12 同步与命令录制** |
| **M2 网格与相机** | Assets(导入 OBJ/glTF + 贴图)+ Renderer ForwardPass + Camera + 纹理模型旋转 | 资源管线、相机变换 |
| **M3 Scene + 多对象** | Entity/Transform 层级 + Component + System,多对象,RenderSystem 收集提交 | 场景模型、System |
| **M4 光照与材质** | 基础 Blinn-Phong 或简化 PBR + 多光源 + 材质系统 | 光照、材质 |
| **M5 Editor** | 集成 ImGui,Hierarchy/Inspector/Stats 面板 | 调试工具链 |
| **M6+(可选)** | Physics、阴影、RenderGraph、异步资源/热重载、向 ECS 存储演进 | 进阶子系统 |

## 12. 下一步

按里程碑顺序,从 **M0** 开始,各里程碑各自走 `writing-plans → 实现` 流程。
