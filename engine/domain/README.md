# me_domain — 农场领域层

农场模拟的领域玩法(纯 CPU,可 doctest)。依赖单向:`me_domain → me_core + nlohmann_json`。

## M8.1 时间系统
- `TimeConfig` / `LoadTimeConfig`:数据驱动的四级日历参数(分钟/天/季/年),从 JSON 加载 + 校验。
- `TimeSystem`:单调 `totalMinutes_` 为单一真相源;`Advance(minutes)` / `Update(realDeltaSeconds)` 返回 `TimeStep` 践跳计数;`Now()` 派生 `CalendarTime`。

### 计数基准约定
- `season`:0 基索引(`seasonNames` 下标)。
- `dayOfSeason`:1 基(`Spring 1` 式)。
- `minuteOfDay`:0 基,`[0, minutesPerDay)`;`hour = minuteOfDay/60`、`minute = minuteOfDay%60`。
- `year`:从 `startYear` 起算。

时间是运行时状态,**不进 Command/Undo**(见 ADR 0006)。

## 作物生长(M8.2)

- `CropConfig` / `CropDatabase`(`CropConfig.h`):数据驱动作物表,`LoadCropDatabase(json)` 返回 `std::optional`(顶层数组、字段校验、id 唯一)。
- `FarmField`(`FarmField.h`):以 `TileKey` 为键的作物实例网格 + 浇水驱动生长状态机。`Plant`/`Water`/`AdvanceDays`/`Harvest`/`At`/`Crops`/`Database`;值语义可拷贝供 Tool dry-run。
- 运行时态,不进 Command/Undo(见 ADR 0007)。经 toolapi 的 `crop.*` 5 Tool 暴露。

## 库存/物品系统(M8.3)

- `ItemConfig` / `ItemDatabase` / `InventoryConfig` / `LoadInventoryConfig`(`ItemConfig.h`):
  数据驱动物品表,`LoadInventoryConfig(json)` 返回 `std::optional<InventoryConfig>`,
  顶层对象/`capacity ≥ 1`/每项 `id`、`name`、`category` 非空/`maxStack ≥ 1`/
  `sellPrice ≥ 0`/`id` 唯一全校验;`ItemDatabase` 提供 `Find(id)` 与 `Size()`。
- `Inventory`(`Inventory.h`):固定格位网格(`std::vector<ItemStack>`,确定性遍历);
  `Add`/`Remove` **全量或不加/不减(all-or-nothing)**,总容纳量不足时零状态变更;
  `CanAdd(itemId, count)` 非破坏性预判,供 `crop.harvest` 原子性检查;
  值语义可拷贝(`ItemDatabase` + `vector<ItemStack>` 均为值类型),供 Tool dry-run。
- `FarmField::PeekHarvest(x,y)`:非破坏性收获预判(只判空/熟 + 计算 `itemId/yield`,
  不清瓦片),供 `crop.harvest` 原子直写库存:"库满 → harvest 失败且瓦片不清"的
  原子性由 `PeekHarvest + CanAdd` 两步保证。
- 运行时态,不进 Command/Undo(见 ADR 0009)。经 toolapi 的 `inventory.*` 3 Tool 暴露
  (`get`[AgentAllowed] / `add`[Automation] / `remove`[EditorOnly])。
