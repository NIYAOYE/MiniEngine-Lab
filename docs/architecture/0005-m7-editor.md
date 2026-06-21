# ADR 0005 — M7 Editor as Client 架构决策

- **状态**: 已接受
- **日期**: 2026-06-21
- **里程碑**: M7 Editor as Client(精简切片)

---

## 背景

M6 建成了受控 Tool API(6 Tool:3 query + 3 mutation,变更经 CommandStack 可 Undo)。M7 的唯一目标是**用引擎自己的编辑器作为 Tool API 的第一个客户端,反向验证接口完备性**:如果一个真实编辑器能只靠这套接口完成"实体增删 + 变换编辑 + 层级浏览 + 审计查看",接口就被证明可用;凡接口给不出的数据/能力,即暴露为下一批 Tool 的明确需求。本里程碑仍不接大模型。需要决策五件事:编辑器与引擎的对话方式、可测试性架构、宿主形态、范围、以及如何处理验证过程暴露的缺口。

---

## 决策一:EditorController 读写都只经 ToolRegistry::Invoke

**决策**:新建 CPU-only 静态库 `me_editor`,核心是 `EditorController`,**读(查询 Tool)写(变更 Tool)都只经 `ToolRegistry::Invoke`** 与引擎对话,固定 `CallerRole::Editor`。面板需要的所有数据来自 `scene.list_entities` / `scene.get_entity` / `log.read` 的 JSON 结果,解析为 DTO(`EntityRow` / `EntityDetails` / `LogRow`)。

**理由**:项目设计哲学允许"只读展示直接查询 Scene",但本里程碑**主动选择更严格的"读也走 Tool"**——直接读 Scene 会绕过被验证的对象,使反向验证失去意义。读走 Tool 才能逼出查询接口的缺口(见决策五)。

**替代方案**:面板直接读 Scene、只有变更走 Tool。更省事,但只验证了一半接口(变更面),放弃了 M7 的核心价值。

---

## 决策二:剖分可测的 EditorController,ImGui 封死在 sandbox 边界

**决策**:`me_editor` 只依赖 `toolapi → command → scene → core` + nlohmann/json,**不依赖 ImGui、不依赖 RHI**,因此可在 WSL doctest 红绿(13 个 `EditorController:*` 用例)。ImGui DX12+Win32 后端与四面板绘制留在 `sandbox/main.cpp`,是目视层。

**理由**:延续 M4 ADR「`RenderView→SpriteBatch` 桥接暂放 sandbox」与项目「RHI/Renderer 不强求自动化、靠 sandbox 目视」的处置。把"UI 动作→Tool 调用"的决策逻辑剖出来单测,保住红绿纪律;ImGui 与 DX12 一样封死在应用边界,不外泄到引擎库。

**替代方案**:面板函数直接在 ImGui 回调里调 `ToolRegistry.Invoke`,不抽 controller。代码更少,但编辑逻辑与 RHI 一样只能目视,无法 WSL 单测。

---

## 决策三:宿在现有 sandbox 主循环,不提前造 Engine/Layer 栈

**决策**:在现有 `sandbox/main.cpp` 的 DX12 主循环里接入 ImGui(NewFrame→面板→Render),不建 spec §7 设想的 `Application` + `Layer` 栈。

**理由**:延续 M4 ADR「无独立 Engine 层」。Engine/Layer 抽象是 app 框架工作,与"验证 Tool API"正交;YAGNI,等主循环增长到值得提取时再建。

---

## 决策四:范围 = 精简切片(只用现有 6 Tool,零序列化)

**决策**:首个 M7 切片只用 M6 的 6 个 Tool:Hierarchy(list_entities)+ Inspector(get_entity / set_transform 编辑)+ Create/Destroy 按钮 + Undo/Redo + Stats + Log(log.read)。TileMap 编辑(`tilemap.paint`)、存档(`scene.save`/`load` + 序列化)、组件编辑(`entity.add_component`)推迟。

**理由**:`create→set_transform→destroy→undo` 往返是接口完备性最强证明,与 ADR 0003/0004 一脉相承。新 Tool 依赖未落地子系统(瓦片编辑数据模型、场景序列化),YAGNI。

---

## 决策五:把 Tool API 缺口显式记录为反向验证发现(本切片不补 Tool)

**决策**:精简切片**不补 Tool**,但把验证过程暴露的缺口显式记录为 M7 交付物:

1. **Undo/Redo 未暴露为 Tool**:M6 的 CommandStack 在 ToolContext 内但无 `edit.undo`/`edit.redo` Tool。编辑器作为特权宿主**直接调 `ctx.commands.undo/redo`**。→ 后续候选 Tool `edit.undo` / `edit.redo`(届时 Agent 也能受控撤销)。
2. **实体无人类可读 label**:`list_entities` / `get_entity` 只回 EntityId,Hierarchy 只能显示 `Entity #<id>`。→ 后续候选 `entity.set_name` + get/list 增 name 字段。
3. **`scene.get_entity` 不返回组件列表**:只回 transform + parentId + children,Inspector 无法列出/编辑组件。本切片 Inspector 只编辑 transform。→ 后续候选 get_entity 增 `components` 字段 + `entity.add_component` / `entity.remove_component`。

**理由**:这些发现正是"Editor as Client 反向验证接口完备性"的具体兑现,而非实现瑕疵。M6 的 6 Tool 足以驱动一个最小编辑器;组件编辑与命名是下一批 Tool 的明确需求来源,且都需要先落地对应的引擎能力/命令,符合 YAGNI 与"变更必经 Command"的约束。

---

## 影响

- 新增 `me_editor`(CPU-only,WSL 可单测);依赖图新增一条 `editor → toolapi`(严格单向不变)。
- 新增 Dear ImGui v1.91.5 依赖(URL 钉版,仅 Windows 构建,封死在 sandbox)。
- `platform::Window` 新增裸类型 Win32 消息钩子(不依赖 ImGui),供后端拦截消息。
- 验证策略:`EditorController` 全链路 CPU-only doctest 红绿(WSL);ImGui 面板与 DX12 录制靠 sandbox 真机目视(pending-user,同 RHI 惯例)。
- M7 产出三条 Tool 缺口,转为 M8+ 的接口演进需求(`edit.undo/redo`、`entity.set_name`、`get_entity.components` + `add_component`)。
