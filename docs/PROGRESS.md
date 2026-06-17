# MiniEngine 开发进度

> 跨会话进度索引。**每次有实质进展时更新本文件**:勾选里程碑、更新"当前状态"和"下一步"、追加决策记录。

- **最后更新**:2026-06-17
- **当前阶段**:设计完成,尚未开始编码(M0 未启动)
- **方向**:Agent-ready Engine API —— 学习型 3D 游戏引擎(C++/DX12),引擎能力抽象为受控、可验证的 Tool API

## 一句话现状

架构设计已确认并定稿,下一步是为 **M0 地基**编写实现计划(`writing-plans`)后开始编码。当前仓库仅有文档,无源码。

## 文档索引

| 文档 | 用途 |
|------|------|
| [架构设计](superpowers/specs/2026-06-17-miniengine-design.md) | 权威设计文档:分层架构、Tool API、Command、路线图 |
| 本文件 `docs/PROGRESS.md` | 跨会话进度追踪 |
| `../CLAUDE.md` | 项目定位与代码生成规则(随代码落地后更新) |

## 里程碑进度

图例:☐ 未开始 ◐ 进行中 ☑ 完成

| 里程碑 | 状态 | 说明 |
|--------|------|------|
| 架构设计 | ☑ | 已确认并提交(转向 Agent-ready) |
| **M0 地基** | ☐ | CMake 骨架 + Core(Math/Log/Handle)+ Platform(窗口/输入/计时)+ 单测 |
| M1 三角形上屏 | ☐ | RHI 最小可用(Device/SwapChain/CmdList/Fence/PSO)→ 清屏 → 三角形 |
| M2 网格与相机 | ☐ | Assets 导入 + ForwardPass + Camera + 纹理模型 |
| M3 Scene + 多对象 | ☐ | Entity/Transform/Component/System |
| M3.5 Command 中枢 | ☐ | ICommand + CommandStack + Undo/Redo |
| M4 ToolAPI | ☐ | ToolRegistry + Tool + JSON Schema 校验 + dry-run + 日志 + 首批 Tool + 白名单 |
| M5 Editor as Client | ☐ | ImGui 面板改为通过 Tool API 操作 |
| M6 光照与材质 | ☐ | Blinn-Phong / 简化 PBR + 多光源 |
| M7+ 未来 | ☐ | Physics、阴影、RenderGraph、异步资源;未来 Agent/LLM 接入 |

> 注:当前阶段交付止于 M5(只建接口层,不接大模型)。

## 下一步行动

1. 选定起点:从 **M0 地基**顺序推进,或优先验证 **ToolAPI 主线(M3.5 + M4)**。
2. 对所选里程碑调用 `writing-plans` 生成实现计划。
3. 按计划实现并回写本文件进度。

## 关键决策记录(ADR 摘要)

| 日期 | 决策 | 理由 |
|------|------|------|
| 2026-06-17 | 实体模型用混合方案(OOP 层级 + 数据组件 + System) | 直观好学,预留 ECS 演进路径 |
| 2026-06-17 | 技术栈 C++ + DirectX 12,Windows,3D | 学习现代图形 API |
| 2026-06-17 | 转向 Agent-ready Tool API | 编辑期能力统一为受控、可验证接口 |
| 2026-06-17 | 双轨:运行时直调 C++,编辑期走 Tool API | 兼顾性能与受控 |
| 2026-06-17 | Tool 参数/结果用 JSON + JSON Schema 校验 | 适配未来 LLM 工具调用 |
| 2026-06-17 | 变更型 Tool 必经 Command(可 Undo/dry-run) | 可审计、可预览、可回滚 |

## 待解决 / 开放问题

- 起始里程碑未定(M0 顺序推进 vs. 优先 ToolAPI 主线)。
- JSON Schema 校验具体选库或手写,M4 时确定。
