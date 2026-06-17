# MiniEngine 开发进度

> 跨会话进度索引。**每次有实质进展时更新本文件**:勾选里程碑、更新"当前状态"和"下一步"、追加决策记录。

- **最后更新**:2026-06-17
- **当前阶段**:设计完成,尚未开始编码(M0 未启动)
- **方向**:2D/2.5D 农场模拟游戏引擎(C++/DX12 + Win32),引擎能力抽象为受控、可验证的 Agent-ready Tool API

## 一句话现状

架构设计已确认并定稿(融合方向:2D/2.5D 农场模拟 + Agent-ready Tool API),下一步是为 **M0 地基**编写实现计划(`writing-plans`)后开始编码。当前仓库仅有文档,无源码。

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
| **M0 地基** | ☐ | CMake 骨架 + Core(2D Math/Log/Handle)+ Platform(窗口/输入/计时)+ 单测 |
| M1 精灵上屏 | ☐ | RHI 最小可用(Device/SwapChain/CmdList/Fence/PSO)→ 清屏 → 带纹理精灵 |
| M2 批渲染 + 正交相机 | ☐ | SpriteBatch 合批 + 正交相机 + 多精灵 |
| M3 瓦片地图 | ☐ | Tileset + TileMap 渲染 + 从 JSON 加载地图 |
| M4 Scene + 组件 | ☐ | Entity/Transform2D + Component + System |
| M5 Command 中枢 | ☐ | ICommand + CommandStack + Undo/Redo |
| M6 ToolAPI | ☐ | ToolRegistry + Tool + JSON Schema 校验 + dry-run + 日志 + 首批 Tool + 白名单 |
| M7 Editor as Client | ☐ | ImGui 面板(含瓦片地图编辑)改为通过 Tool API 操作 |
| M8 农场领域层 | ☐ | 时间系统(天/季节)+ 作物生长 + 库存/物品(JSON)+ NPC 日程调度 |
| M9+ 未来 | ☐ | 对话/配方数据驱动、2D 物理/碰撞、存档;未来 Agent/LLM 接入白名单 Tool |

## 下一步行动

1. 选定起点:从 **M0 地基**顺序推进,或优先验证 **ToolAPI 主线(M5 + M6)**。
2. 对所选里程碑调用 `writing-plans` 生成实现计划。
3. 按计划实现并回写本文件进度。

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

## 待解决 / 开放问题

- 起始里程碑未定(M0 顺序推进 vs. 优先 ToolAPI 主线)。
- JSON Schema 校验具体选库或手写,M6 时确定。
- 瓦片地图是否兼容 Tiled JSON 格式,M3 时确定。
