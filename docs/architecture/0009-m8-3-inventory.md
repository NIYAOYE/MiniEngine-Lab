# ADR 0009:M8.3 库存/物品系统

- 日期:2026-06-23
- 状态:已采纳
- 相关:[ADR 0007 作物生长](0007-m8-2-crop-growth.md)、[M8.3 设计](../superpowers/specs/2026-06-23-m8-3-inventory-design.md)

## 背景

M8.2 落地 `FarmField`/`CropDatabase` 与 5 crop Tool。M8.3 在其上闭合农场核心循环:
实现**数据驱动格位库存**,并将 `crop.harvest` 产出**直写库存**——"种植→浇水→成熟→收获入库"
全流程贯通。逻辑全部为纯 CPU 领域代码,可在 WSL doctest 红绿。

## 决策

1. **库存用格位网格**(固定 `capacity` 个 slot,每 slot `{itemId, count ≤ maxStack}`),
   非简单计数表。贴近真实农场模拟 UI(Stardew Valley 风);`capacity` 由 `items.json`
   数据驱动,源码零硬编码。`ItemDatabase` 为 `id → ItemConfig` 只读映射,`LoadInventoryConfig`
   返回 `std::optional<InventoryConfig>`(含 `capacity` + `ItemDatabase`),顶层
   非对象/字段非法/id 重复均返回 `nullopt`。

2. **`Add`/`Remove` 全量或不加/不减(all-or-nothing)**。`Add` 先补同物品未满堆、
   再占空格;总可容纳量 < count 时**零状态变更**(返回 `Full`);`Remove` 持有量不足时
   同样**零状态变更**(返回 `NotEnough`)。此语义让 `crop.harvest` 的原子性成为
   一行 `CanAdd` 预判,避免"部分填充 + 溢出剩余"的复杂性,最小正确。

3. **`crop.harvest` 直写库存,经 `FarmField::PeekHarvest` + `Inventory::CanAdd` 预判
   保证原子性**。新流程:① `PeekHarvest(x,y)` 非破坏性判空/熟并计算 `itemId/count`;
   ② 若 `ctx.inventory && !ctx.inventory->CanAdd(peek.itemId, peek.count)` →
   返回 `PreconditionFailed "inventory full"`,瓦片**不清**;③ `Harvest(x,y)`
   清瓦片;④ 若 `ctx.inventory` 非空则直调 `ctx.inventory->Add`(内部引擎调用,
   **不经 Tool 权限白名单**)。`Harvest` 内部复用 `PeekHarvest`,零重复校验逻辑。

4. **`ItemConfig` 含 `sellPrice`(未来商店预留),不含 `icon`**。商店/经济系统为后续
   里程碑,`sellPrice` 字段占位不阻塞加载;`icon`/物品贴图渲染无当前消费者,YAGNI。

5. **inventory 变更不经 `CommandStack`**;dry-run 用值拷贝副本预演。库存增删是运行时
   游戏态,与 `time.advance`/crop 变更同类。沿用 **ADR 0006/0007 文档化例外**("变更经
   Command"是场景编辑约定,不适用运行时态)。`Inventory` 值语义可拷贝(成员皆值类型),
   Tool dry-run 在副本上预演,同 `FarmField`。**不可 Undo**。

6. **权限梯度 `get=AgentAllowed / add=Automation / remove=EditorOnly`**。查询只读给
   最宽松档;`add` 是自动化常规变更;`remove` 销毁性(丢弃物品,不可 Undo),收紧到
   `EditorOnly`,与 `crop.harvest`/`scene.destroy_entity` 同档,演示三层白名单梯度。
   Builtin Tool 总数 **13 → 16**。

7. **items 与 crops 独立加载 + 交叉软告警**(`harvestItemId` 缺失 → 测试断言 + 启动时
   `ME_LOG_WARN`,不阻断启动)。两表独立加载避免领域间硬耦合;`crops.json` 中每个
   `harvestItemId` 期望在 items 表找到,缺失时 `Inventory::Add` 因未知物品失败
   (可观测),不硬失败允许配置逐步迁移。

8. **`ctx.inventory == nullptr` 向后兼容**。`crop.harvest` 在 `ctx.inventory` 为
   `nullptr` 时照常返回产出(`addedToInventory:false`),M8.2 既有 5 条 crop 测试
   (未接库存)零回归。

## 后果

- M8.3 在 WSL/MSVC CPU-only doctest 231/231 全绿(无 Windows/GPU 依赖);
  基线 208 + 新增 ItemConfig 2 + Inventory 7 + FarmField PeekHarvest 2 +
  InventoryTools 8 + CropTools harvest→inventory 4 = 23 用例。
- "种植→浇水→成熟→收获入库"农场核心循环在 C++ 领域层完全贯通。
- 前端/UI 消费库存、商店/经济/卖出、库存进 Undo 均为后续里程碑(YAGNI)。
- `assets/config/items.json` 数据驱动(`capacity:36`,覆盖 crops.json 全 `harvestItemId`);
  toolserver_app 同步加载 items.json,缺失/非法退出,同 crops.json/time.json。
