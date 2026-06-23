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
