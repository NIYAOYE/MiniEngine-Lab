# MiniEngine 开发进度

> 跨会话进度索引。**每次有实质进展时更新本文件**:勾选里程碑、更新"当前状态"和"下一步"、追加决策记录。

- **最后更新**:2026-06-20
- **当前阶段**:M3 瓦片地图完成,下一步 M4 Scene + 组件
- **方向**:2D/2.5D 农场模拟游戏引擎(C++/DX12 + Win32),引擎能力抽象为受控、可验证的 Agent-ready Tool API

## 一句话现状

M1 精灵上屏已完成:Win32 Window/Input + RHI(GpuDevice/SwapChain/CommandContext/Fence/DescriptorHeap/GpuBuffer/GpuTexture/Shader/PSO)+ stb_image 纹理解码 + SpriteRenderer(根签名+PSO+单位四边形,经 MVP 绘制)。跨平台逻辑在 WSL 用 doctest 红绿;DX12/GPU 正确性由 WARP 软件适配器 + 离屏像素回读自动化把关(me_gpu_tests:设备/围栏、纹理上传往返、带纹理精灵渲染),并经 sandbox 真机目视确认(开窗、贴图正立、WASD 平移、ESC 退出)。M2 批渲染 + 正交相机已完成:SpriteBatch 按纹理合批 + OrthographicCamera + 8×5 多精灵 sandbox;WARP 多精灵/色调/srcRect 像素回读 + 相机 doctest + sandbox 目视验证。M3 瓦片地图已完成:TileLayout(gid→UV)/TileGeometry(col/row→worldRect)/Tileset/TileMapRenderer + nlohmann/json Tiled JSON 子集加载器(LoadTiledMap) + sandbox 数据驱动演示(12×8 地图、2 层、相机平移/缩放);WARP 像素回读 + doctest 单测 + sandbox 目视(pending-user)。

## 文档索引

| 文档 | 用途 |
|------|------|
| [架构设计](superpowers/specs/2026-06-17-miniengine-design.md) | 权威设计文档:分层架构、2D 渲染、Tool API、Command、农场领域层、路线图 |
| 本文件 `docs/PROGRESS.md` | 跨会话进度追踪 |
| `../CLAUDE.md` | 项目定位与代码生成规则(本地文件,被 .gitignore 忽略) |

## 里程碑进度

图例:☐ 未开始 ◐ 进行中 ☑ 完成。当前阶段交付止于 M7(接口层 + 编辑器,不接大模型);农场领域层 M8 起。

| 里程碑 | 状态 | 说明 |
|--------|------|------|
| 架构设计 | ☑ | 已确认并提交(融合:2D/2.5D 农场 + Agent-ready Tool API) |
| **M0 地基** | ☑ | CMake 骨架 + Core(2D Math/Log/Handle/Assert)+ Platform(计时/文件系统)+ doctest 单测;Win32 窗口/输入推迟到 M1 |
| **M1 精灵上屏** | ☑ | Win32 Window/Input + RHI(Device/SwapChain/CmdList/Fence/PSO)+ stb_image 纹理 + SpriteRenderer;WARP 像素回读 + sandbox 目视验证 |
| **M2 批渲染 + 正交相机** | ☑ | SpriteBatch 按纹理合批 + OrthographicCamera + 多精灵;WARP 多精灵/色调/srcRect 像素回读 + 相机 doctest + sandbox 目视 |
| **M3 瓦片地图** | ☑ | Tileset + TileMap 渲染 + 从 JSON 加载地图;sandbox 数据驱动演示;sandbox 目视 pending-user |
| M4 Scene + 组件 | ☐ | Entity/Transform2D + Component + System |
| M5 Command 中枢 | ☐ | ICommand + CommandStack + Undo/Redo |
| M6 ToolAPI | ☐ | ToolRegistry + Tool + JSON Schema 校验 + dry-run + 日志 + 首批 Tool + 白名单 |
| M7 Editor as Client | ☐ | ImGui 面板(含瓦片地图编辑)改为通过 Tool API 操作 |
| M8 农场领域层 | ☐ | 时间系统(天/季节)+ 作物生长 + 库存/物品(JSON)+ NPC 日程调度 |
| M9+ 未来 | ☐ | 对话/配方数据驱动、2D 物理/碰撞、存档;未来 Agent/LLM 接入白名单 Tool |

## 下一步行动

1. 对 **M4 Scene + 组件** 调用 `writing-plans` 生成实现计划(Entity/Transform2D + Component + System)。
2. 按计划实现并回写本文件进度。
3. ToolAPI 主线(M5 + M6)为后续受控编辑能力的重点。

## 关键决策记录(ADR 摘要)

| 日期 | 决策 | 理由 |
|------|------|------|
| 2026-06-17 | 实体模型用混合方案(OOP 层级 + 数据组件 + System),不用 EnTT | 直观好学,预留 ECS 演进路径 |
| 2026-06-17 | 底座 C++ + DirectX 12 + Win32 | 学习现代图形 API |
| 2026-06-17 | 转向 Agent-ready Tool API | 编辑期能力统一为受控、可验证接口 |
| 2026-06-17 | 双轨:运行时直调 C++,编辑期走 Tool API | 兼顾性能与受控 |
| 2026-06-17 | Tool 参数/结果用 JSON + JSON Schema 校验 | 适配未来 LLM 工具调用 |
| 2026-06-17 | 变更型 Tool 必经 Command(可 Undo/dry-run) | 可审计、可预览、可回滚 |
| 2026-06-17 | **融合定调:维度改为 2D/2.5D 农场向** | 目标品类是星露谷式农场模拟;简化渲染/数学/物理 |
| 2026-06-17 | **去掉 SDL2**,渲染/输入用 DX12 + Win32 | 保留 Agent-ready/DX12 底座 |
| 2026-06-17 | **农场领域模块(时间/瓦片玩法/NPC 日程)纳入后期里程碑(M8)** | 引擎核心保持通用,领域玩法独立成层 |
| 2026-06-17 | M0 Core/Platform 跨平台,WSL 可单测;Win32 Window/Input 推迟到 M1 | Core/计时/文件系统不依赖窗口,可即时 TDD;窗口需真实 Windows 验证,与 DX12 上屏合并 |
| 2026-06-17 | 数学库定为行主序 + 行向量(p' = p*M) | 与 DX12/DirectXMath 同源,降低 M1 接图形 API 的心智负担 |
| 2026-06-17 | 单测框架选 doctest(FetchContent) | 单头、编译快、API 简洁,契合最小依赖 |
| 2026-06-19 | M1 着色器用 FXC(D3DCompileFromFile,SM5.1)而非 DXC | 零额外依赖、最快上屏;DXC 留到需 SM6 时 |
| 2026-06-19 | M1 用系统 DX12(d3d12/dxgi/d3dcompiler),不引入 Agility SDK | 最小依赖,够用即可 |
| 2026-06-19 | GPU 代码用 WARP 软件适配器 + 离屏像素回读做自动化测试,辅以 sandbox 目视 | 无独显/无窗口环境也能红绿,补足 RHI 不可纯 CPU 单测的空缺 |
| 2026-06-19 | Windows 侧定义 UNICODE/_UNICODE | 全程用 -W API + 宽字符串,使资源宏(IDC_ARROW)解析为宽字符版本 |
| 2026-06-19 | 合批策略 = 按纹理指针稳定排序后逐 run 合 drawcall;模型变换 CPU 端烘入顶点 | 合批无法每精灵传根常量,CPU 侧烘入顶点无额外 GPU 开销;稳定排序保持同纹理提交顺序 |
| 2026-06-19 | 顶点格式 pos+uv+color 一次到位,srcRect 支持图集采样 | color 色调支持 M2 测试用例;srcRect UV 子区域供 M3 瓦片图集复用 |
| 2026-06-19 | M1 `SpriteRenderer` 退役并入 `SpriteBatch` | 消除重复根签名/PSO/VB 逻辑;单一渲染路径降低维护成本 |
| 2026-06-19 | 延续上传堆 + 每帧全同步;帧并行(FrameRing)/默认堆迁移推迟到性能里程碑 | M2 目标是合批正确性与相机;提前引入 per-frame fence 会增加调试复杂度,与本里程碑目标无关 |
| 2026-06-19 | 容量溢出策略 = VB/IB 按高水位自动增长(不静默丢弃) | 静默丢弃会导致渲染缺失难以排查;自动增长保证正确性,性能影响在可接受范围内 |

| 2026-06-20 | Tiled JSON 子集:只支持 orthogonal / tilelayer / 单嵌入 tileset | 覆盖农场模拟所需场景;复杂子集(objectgroup、多 tileset)推迟到需求明确后 |
| 2026-06-20 | TileMapRenderer 复用 SpriteBatch(不自行 Begin/End) | 瓦片与精灵同帧内混合提交;单 tileset 全合批 → 1 drawcall |
| 2026-06-20 | JSON 解析库选 nlohmann/json v3.11.3(FetchContent) | 头文件单一、API 简洁、零异常模式(`get_to`)符合项目规范 |

## 待解决 / 开放问题

- JSON Schema 校验具体选库或手写,M6 时确定。
- 瓦片地图是否兼容 Tiled JSON 格式,M3 时确定。
- 上传堆顶点/纹理为 M1 简化(可读性优先);仍延续,推迟到后续性能里程碑(M2 已确认不做默认堆迁移)。
- M1 每帧 `fence->Flush` 全同步(无帧并行);仍延续,推迟到后续性能里程碑(M2 已确认不做帧并行)。
