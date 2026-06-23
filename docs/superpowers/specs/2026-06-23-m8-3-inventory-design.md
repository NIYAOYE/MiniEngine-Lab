# M8.3 库存/物品系统 — 设计文档

- **日期**:2026-06-23
- **里程碑**:M8.3(农场领域层 M8 第三切片,承接 M8.2 作物生长)
- **状态**:已确认,待写实现计划
- **前置**:M8.2 作物生长(`me_domain` 模块、`FarmField`/`CropDatabase`、5 crop Tool);M9.1 Tool HTTP 传输层(toolserver 装配点)

## 1. 目标与范围

实现**数据驱动的格位库存系统**:物品表从 JSON 加载,库存为固定格位网格(Stardew 风),
经受控 Tool API 增删查;并把 **`crop.harvest` 的产出直接写入库存**,闭合"种植→浇水→
成熟→收获入库"的农场核心循环。逻辑全部为纯 CPU 领域代码,可在 WSL 用 doctest 红绿。

### 明确不做(YAGNI,留后续里程碑)

- 前端/UI 消费库存:本里程碑只做 C++ 领域 + Tool API + 资产 + 测试,**不动 `tools/editor-frontend`**(沿 M8.2 在领域里程碑不触前端的先例)。
- 商店/经济/卖出:`sellPrice` 字段为未来预留,本里程碑无消费者。
- `icon`/物品贴图渲染:本里程碑纯逻辑,不引入。
- 库存进 Command/Undo:运行时游戏态,沿 ADR 0006/0007 例外,不可 Undo。
- EditorController 扩展:M7 已封口,不强制扩展。
- Windows/GPU 构建不阻塞本里程碑(纯逻辑)。

## 2. 架构与模块归属

新增逻辑落在 **`me_domain`**(CPU-only,沿 `FarmField`/`TimeSystem` 先例)与
**`me_toolapi`**(三 Tool + ToolContext 扩展)。依赖图不变:

```
me_domain  → me_core + nlohmann/json          (不依赖 Scene / RHI / ImGui)
me_toolapi → me_command + me_scene + me_core + nlohmann/json
```

新增/改动:

| 新增 | 位置 | 类比 |
|------|------|------|
| `ItemConfig` / `ItemDatabase` + `InventoryConfig` + `LoadInventoryConfig` | `engine/domain`(`ItemConfig.h`/`.cpp`) | `CropConfig`/`CropDatabase` |
| `Inventory`(格位网格 + `ItemStack`) | `engine/domain`(`Inventory.h`/`.cpp`) | `FarmField` |
| `inventory.get/add/remove` 三 Tool | `engine/toolapi`(`InventoryTools.cpp`) | crop 五 Tool |
| `ToolContext.inventory*` | `engine/toolapi`(`ToolContext.h`) | `ToolContext.farm*` |
| `FarmField::PeekHarvest` + `crop.harvest` 改造 | `engine/domain` + `engine/toolapi` | — |
| `assets/config/items.json` | assets | `crops.json` |

## 3. 数据模型

### 3.1 配置(`items.json` → `InventoryConfig`)

**顶层为对象**,同时携带格位数与物品表(与 `crops.json` 的纯数组不同——库存容量也要数据驱动):

```jsonc
{
  "capacity": 36,
  "items": [
    { "id": "parsnip", "name": "Parsnip", "category": "crop", "maxStack": 99, "sellPrice": 35 }
  ]
}
```

```cpp
struct ItemConfig {
    std::string id;        ///< 唯一非空标识
    std::string name;      ///< 展示名(非空)
    std::string category;  ///< 分类(如 "crop" / "tool" / "resource";非空)
    int maxStack = 0;      ///< 单格最大堆叠(≥1)
    int sellPrice = 0;     ///< 卖出单价(≥0;未来商店用,本里程碑无消费者)
};

struct InventoryConfig {
    int capacity = 0;      ///< 库存格位数(≥1)
    ItemDatabase items;    ///< id → ItemConfig 只读表
};
```

`ItemDatabase` 提供 `const ItemConfig* Find(const std::string& id) const`(未命中 nullptr)、`Size()`。

**校验约束**(任一不满足 → `nullopt`):

- 顶层必须是对象;`capacity` 为整数且 ≥ 1;`items` 为数组。
- 每项:`id`/`name`/`category` 为非空字符串;`maxStack` 整数 ≥ 1;`sellPrice` 整数 ≥ 0。
- `id` 在表内唯一(重复 → `nullopt`)。

`LoadInventoryConfig(const nlohmann::json&) -> std::optional<InventoryConfig>`,不抛异常,
风格完全对齐 `LoadCropDatabase` / `LoadTimeConfig`。

**与 crops 的交叉校验(软告警,不硬失败)**:`crops.json` 中每个 `harvestItemId` 期望能在
items 表找到。两表**独立加载**(避免领域间硬耦合);缺失项在**测试中断言 + 启动时 `ME_LOG_WARN` 告警**,
不阻断启动——收获仍会产出该 itemId,只是入库时 `Inventory::Add` 因未知物品而失败(可观测)。

### 3.2 运行时格位(`ItemStack` / `Inventory`)

```cpp
struct ItemStack {
    std::string itemId;   ///< 空格:itemId 为空
    int count = 0;        ///< 空格:0;非空时 1..maxStack
    bool Empty() const { return count == 0; }
};
```

`Inventory` 内部:`std::vector<ItemStack> slots_`(固定 `capacity_` 个,确定性遍历,doctest 易断言)。

## 4. Inventory 操作语义

```cpp
class Inventory {
public:
    Inventory(ItemDatabase db, int capacity);          // 值持有 db + 固定格位数

    AddOutcome    Add(const std::string& itemId, int count);    // 全量或不加
    RemoveOutcome Remove(const std::string& itemId, int count); // 全量或不减
    bool          CanAdd(const std::string& itemId, int count) const; // 不改状态预判

    int CountOf(const std::string& itemId) const;      // 跨格位总量
    const std::vector<ItemStack>& Slots() const;       // 只读遍历
    int Capacity() const;
    const ItemDatabase& Database() const;

private:
    ItemDatabase db_;
    std::vector<ItemStack> slots_;
};
```

(`AddOutcome` / `RemoveOutcome` 为轻量结果类型,携带状态枚举 + 说明,由 Tool 层翻译为
结构化 `ToolResult`;具体形态在实现计划细化。)

### 4.1 `Add(itemId, count)` —— 全量或不加(all-or-nothing,**决策 A**)

1. `itemId` 不在 `db_` → `UnknownItem`,不改状态。
2. 否则按 `maxStack = db_.Find(itemId)->maxStack` 计算可容纳量:
   先填**同物品未满堆**的剩余空间,再算**空格**可放 `maxStack` 的份额。
3. 若总可容纳量 **< count** → `Full`,**不改任何状态**(供 harvest 原子性)。
4. 否则按"先补未满堆、再占空格"落子,返回 `Ok`(实际入库 = count)。

> 比真实星露谷的"部分填充 + 溢出剩余"更简单:让 `crop.harvest` 的"库满则收获失败、
> 瓦片不清"成为一行 `CanAdd` 预判,避免溢出剩余语义。符合最小正确。

`CanAdd(itemId, count)` 复用同一容量计算但**不落子**(harvest 预判用,也供测试)。

### 4.2 `Remove(itemId, count)` —— 全量或不减

- `CountOf(itemId) < count` → `NotEnough`,不改状态。
- 否则从持有该物品的格位依次扣减(确定性顺序),**清空被掏空的格**(`count=0`、`itemId` 清空),返回 `Ok`。

### 4.3 值语义可拷贝

`Inventory` 值语义可拷贝(成员皆值类型),供 Tool dry-run 在副本上预演,零副作用,同 `FarmField`。
不进 CommandStack(运行时态,沿 ADR 0006/0007 例外)。

## 5. crop.harvest 直写库存(原子)

为保证"**库满则收获失败、瓦片不清空**"的原子性,改造收获路径:

### 5.1 `FarmField` 加非破坏性预判

```cpp
HarvestResult PeekHarvest(int x, int y) const;  // 只判空/熟 + 算出 itemId/yield,不清瓦片
HarvestResult Harvest(int x, int y);            // 复用 PeekHarvest,Ok 时清瓦片
```

`Harvest` 内部先 `PeekHarvest`,状态 Ok 才清瓦片——零重复校验逻辑。

### 5.2 `crop.harvest` 新流程

1. `peek = farm.PeekHarvest(x,y)` → `EmptyTile` / `NotMature` 直接报错(零副作用)。
2. 若 `ctx.inventory && !ctx.inventory->CanAdd(peek.itemId, peek.count)` →
   `PreconditionFailed "inventory full"`(瓦片**不清**)。
3. `farm.Harvest(x,y)`(清瓦片);若 `ctx.inventory` 非空则 `ctx.inventory->Add(peek.itemId, peek.count)`。
4. 返回 `{ itemId, count, addedToInventory: <ctx.inventory != nullptr> }`。

- **harvest 内部直调 `Inventory::Add`**(非经 `inventory.add` Tool),属引擎内部调用,
  **不受 Tool 权限白名单影响**(故 `remove=EditorOnly` 等不波及 harvest)。记录在案。
- **向后兼容**:`ctx.inventory == nullptr` 时 harvest 照常返回产出(`addedToInventory:false`),
  M8.2 既有 5 条 crop 测试(未接库存)零回归。
- **dry-run**:同时值拷贝 `farm` 与 `inventory` 在副本上预演,真身不变。

## 6. ToolAPI 接线

### 6.1 `ToolContext` 扩展

与 `time`/`farm` 完全对称,加可选指针(前置声明隔离,不拉入 domain 头):

```cpp
namespace me::domain { class Inventory; }

struct ToolContext {
    me::scene::Scene& scene;
    me::command::CommandStack& commands;
    ToolInvocationLog& log;
    me::domain::TimeSystem* time = nullptr;
    me::domain::FarmField*  farm = nullptr;
    me::domain::Inventory*  inventory = nullptr; ///< 可选:inventory Tool 数据源,缺省 nullptr
};
```

M6/M7/M8.1/M8.2 既有构造保持有效(默认 nullptr),不破坏现有测试。

### 6.2 三个 Tool

新文件 `engine/toolapi/src/tools/InventoryTools.cpp`;工厂声明进 `BuiltinTools.h`;
`RegisterBuiltinTools` 追加注册。Builtin Tool 总数 **13 → 16**。

| Tool | 类别 / 权限 | params schema | 成功结果 |
|------|-------------|---------------|----------|
| `inventory.get` | Query / **AgentAllowed** | `{}` | `{capacity, used, slots:[{slot,itemId,count}]}` |
| `inventory.add` | Mutation / **Automation** | `{itemId:string, count:int≥1}` | 入库后库存视图 |
| `inventory.remove` | Mutation / **EditorOnly** | `{itemId:string, count:int≥1}` | 扣减后库存视图 |

- `slots` 含全部格位(空格 `itemId:""`、`count:0`),`slot` 为下标,确定性顺序;`used` = 非空格位数。
- 全部经既有 `ToolRegistry::Invoke` 流水线(查找→白名单→Schema 校验→dryRun?→run→审计回填 invocationId)。

### 6.3 关键决策:inventory 变更不经 CommandStack

库存增删是**运行时游戏态**,与 `time.advance` / crop 变更同类。沿用 **ADR 0006 文档化例外**:
「变更经 Command」是**场景编辑**约定,不适用运行时状态。这些 Tool 直接经 `ctx.inventory` 落地,**不可 Undo**。

### 6.4 dry-run 零副作用

沿 crop 手法:`me::domain::Inventory preview = *ctx.inventory;` 值拷贝后在副本执行。
`inventory.get` 只读,`dryRun` 即 `run`。所有 Tool 入口先判 `ctx.inventory == nullptr → PreconditionFailed`,同 `ctx.farm` 守卫。

### 6.5 权限梯度理由

- 查询(`get`)给 **AgentAllowed**。
- `add` 是自动化常规变更,给 **Automation**。
- `remove` 销毁性(丢弃物品,运行时不可 Undo),收紧到 **EditorOnly**,与 `crop.harvest` / `scene.destroy_entity` 同档,演示三层白名单梯度。

## 7. 错误处理(不抛异常)

- **配置加载**:`LoadInventoryConfig` 返回 `std::optional`;顶层非对象、`capacity<1`、`items` 非数组、字段缺失/类型错/语义越界、id 重复 → `nullopt`。
- **`Inventory` 域操作**:返回带状态结果,Tool 层翻译为结构化 `ToolResult`:
  - 未知物品 / 库满(Add)/ 数量不足(Remove)→ `PreconditionFailed`(带具体 message)。
  - `ctx.inventory == nullptr` → `PreconditionFailed`。
  - 参数缺失/类型错由 Registry 的 Schema 校验**前置**拦截(`InvalidParams`),Tool 内不重复校验。
- **零魔法数字**:格位数、maxStack、sellPrice、yield 等全部来自配置;源码无裸常量。

## 8. 测试策略(全 CPU-only doctest,WSL 红绿)

预计在现有 208 基础上新增约 20+ 用例。

1. **`ItemConfig` 测试** — 合法 `items.json` 往返;各类非法配置返回 `nullopt`(顶层非对象、缺 capacity、capacity<1、items 非数组、缺字段、类型错、maxStack<1、sellPrice<0、id 重复);与 `assets/config/items.json` 取值严格一致(同 M8.2 `ValidCropJson()` 先例,本里程碑 `ValidItemsJson()`)。
2. **`Inventory` 测试** — Add 先补未满堆再占空格;**全量或不加**(放不下时零状态变更);UnknownItem;CanAdd 与 Add 一致;Remove 不足(NotEnough 零变更)/跨格扣减/清空被掏空格;CountOf;值拷贝独立性(副本改动不影响真身)。
3. **crop.harvest → 库存测试** — harvest 写入库存(`addedToInventory:true`、CountOf 增加);**库满 → harvest 失败且瓦片不清**(原子);`ctx.inventory==nullptr` 时 harvest 仍返回产出(向后兼容);harvest dry-run 对 farm 与 inventory **双零副作用**。
4. **Inventory Tool 测试** — 经 `ToolRegistry::Invoke` 走完整流水线:权限白名单裁决(各角色对 3 Tool,尤其 `remove` 拒绝 Agent、`get` 放行 Agent);Schema 校验(缺 itemId/count、count<1、类型错);**dry-run 零副作用**;`ctx.inventory==nullptr` 守卫;审计日志回填 invocationId。
5. **(可选)ToolDispatcher 贯通** — 镜像 M9.1,加 inventory 端到端 string→string 冒烟用例。

## 9. 资产与装配

- **`assets/config/items.json`** — `capacity` + 物品示例,**至少覆盖 `crops.json` 全部 `harvestItemId`**;取值与 `ItemConfig` 测试的 `ValidItemsJson()` 严格一致(数据驱动,源码零硬编码)。
- **`apps/toolserver/main.cpp`** — 加载 `items.json`(mandatory,缺失/非法 → 退出,同 crops/time)→ 构造 `Inventory(items, capacity)` → `ctx.inventory = &inv`。
- **`CMakeLists.txt`** — `me_domain` 加 `ItemConfig.cpp`/`Inventory.cpp`;`me_toolapi` 加 `InventoryTools.cpp`;对应测试加入 test target。

## 10. 文档

- **ADR 0009**(`docs/architecture/0009-m8-3-inventory.md`)— 记录决策 ①~⑦(见下)。
- **本设计 spec** — `docs/superpowers/specs/2026-06-23-m8-3-inventory-design.md`(本文件)。
- **`engine/domain/README.md`** — 追加 `ItemDatabase`/`Inventory` 说明。
- **`engine/toolapi/README.md`**(若存在)— 追加三 Tool 与 Builtin 总数 16。
- **`docs/PROGRESS.md`** — M8 行更新 M8.3 ☑、一句话现状、下一步改 M8.4(NPC 日程)/库存前端消费、追加 ADR 摘要行。

## 11. 关键决策汇总(转 ADR 0009)

1. 库存用**格位网格**(固定 capacity 个 slot,每 slot `{itemId,count≤maxStack}`),非简单计数表 —— 贴近真实农场模拟 UI;`capacity` 数据驱动。
2. `Add`/`Remove` **全量或不加/不减(all-or-nothing)** —— 让 harvest 原子性成为一行 `CanAdd` 预判,避免溢出剩余语义,最小正确。
3. **`crop.harvest` 直写库存**,经 `FarmField::PeekHarvest` + `Inventory::CanAdd` 预判保证**库满则收获失败、瓦片不清**的原子性;harvest 内部直调 `Inventory::Add`(不经 Tool 权限);`ctx.inventory==nullptr` 向后兼容仍产出。
4. `ItemConfig` 含 `sellPrice`(未来商店预留),**不含 `icon`**(无消费者,YAGNI)。
5. inventory 变更**不经 CommandStack** —— 运行时游戏态,沿 ADR 0006/0007 例外;dry-run 用值拷贝副本。
6. 权限梯度 `get=AgentAllowed / add=Automation / remove=EditorOnly` —— remove 销毁性,与 crop.harvest / scene.destroy_entity 同档。
7. items 与 crops **独立加载 + 交叉软告警**(harvestItemId 缺失测试断言 + 启动 WARN,不硬失败)—— 避免领域间硬耦合,缺失可观测。
