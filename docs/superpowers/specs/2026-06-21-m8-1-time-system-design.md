# M8.1 时间系统设计

- **日期**:2026-06-21
- **里程碑**:M8 农场领域层 → 第一切片 **M8.1 时间系统**
- **状态**:设计已确认,待写实现计划
- **前置**:M0–M6 完成;M7 CPU 核心完成(ImGui 目视层 pending-user)

## 1. 背景与定位

M8 农场领域层在路线图中是一个大里程碑,捆绑了四个**基本独立**的子系统:时间系统、作物生长、库存/物品、NPC 日程调度。本项目遵循 `spec → plan → 实现` 的单切片节奏,故将 M8 拆为多个切片,每个独立成 spec。

**时间系统是共同地基**:作物按天生长、NPC 日程按分钟 key off 时间,而库存/物品与时间最解耦。因此 **M8.1 先做时间系统**。

时间系统是纯 CPU 逻辑,不依赖 GPU/窗口,完全可 doctest(契合权威设计文档 §11「Domain 可单测」)。

## 2. 目标与非目标

### 目标
- 一个数据驱动(JSON)的四级日历:**分钟 / 天 / 季节 / 年**。
- 时间推进的两种入口:运行时按真实帧 delta 推进(`Update`),以及按整分钟显式推进(`Advance`)。
- 推进返回**践跳事件**(`TimeStep`):明示本次推进跨越了多少分钟 / 天 / 季 / 年,供后续作物、NPC 子系统轮询消费。
- 开出最小 Tool API 表面(`time.get` 查询、`time.advance` 推进),延续 Agent-ready 受控接口主线。
- 全程 WSL doctest 红绿。

### 非目标(YAGNI)
- 不做 `time.set`(跳转到任意日期)—— 跳转跨多天的事件语义复杂,等有明确消费场景再加。
- 不做暂停/倍速/天气/节日等玩法层概念。
- 时间**不进 Command/Undo**——它是运行时状态,不是场景编辑;Undo 运行时时间无意义。
- 不引入观察者/回调机制——消费者轮询 `Advance`/`Update` 返回值即可(零全局状态、零回调,契合项目「显式传参」哲学)。

## 3. 模块与依赖

新增 `engine/domain` 模块,产出 `me_domain` STATIC 库(后续承载作物/库存/NPC 子系统)。

- 依赖方向(严格单向):`me_domain → me_core + nlohmann_json`。
- **禁止**依赖 `me_scene / me_rhi / me_renderer / me_toolapi`。
- 目录布局对齐既有模块:`engine/domain/include/me/domain/`、`engine/domain/src/`、`engine/domain/README.md`。
- 测试:新增 `tests/domain` doctest 套件。
- 同步更新根 `CMakeLists.txt`(`add_subdirectory(engine/domain)`)与 `tests/CMakeLists.txt`。

## 4. 内部时间表示(决策:方案 A)

**方案 A —— 单调绝对分钟计数器**(采纳):内部只存一个 `int64 totalMinutes_`(自纪元起的总分钟),其余日历字段按 `TimeConfig` 即时派生。

- 单一真相源,不存在多字段 rollover 的进位错误风险。
- `TimeStep` 的「跨越 N 天/季」由两个绝对计数相减即得,多天跳跃天然支持。

被否方案:
- 方案 B(存 `{minute,day,season,year}` 结构、每次推进进位):状态多、进位逻辑易错。
- 方案 C(存 `double` 秒):精度过剩 + 浮点漂移。

## 5. 数据模型

### 5.1 `TimeConfig`(从 JSON 加载,零硬编码)
| 字段 | 含义 |
|------|------|
| `minutesPerDay` | 一天的游戏分钟数 |
| `daysPerSeason` | 一个季节的天数 |
| `seasonsPerYear` | 一年的季节数 |
| `seasonNames[]` | 季节名列表(数量须 == `seasonsPerYear`) |
| `gameMinutesPerStep` | 一个推进步进的游戏分钟数(如 10) |
| `realSecondsPerStep` | 现实多少秒走一个步进(`Update` 用) |
| `startYear / startSeason / startDay / startMinute` | 初始时间点 |

`std::optional<TimeConfig> LoadTimeConfig(const nlohmann::json&)`:
- 失败返回 `std::nullopt`(不抛异常,契合项目规范)。
- 校验:所有数值字段 > 0(`startMinute` ≥ 0、索引类字段 ≥ 0 按需);`seasonNames.size() == seasonsPerYear`;`startMinute < minutesPerDay`、`startDay`/`startSeason` 在域内。

### 5.2 `CalendarTime`(派生视图,`Now()` 返回)
`{ int year; int season; std::string seasonName; int dayOfSeason; int minuteOfDay; int hour; int minute; }`
- `hour = minuteOfDay / 60`、`minute = minuteOfDay % 60`(便利字段,供 NPC 日程/显示)。
- **计数基准(已落定)**:`season` 为 **0 基索引**(0..seasonsPerYear-1,作数组下标取 `seasonName`);`dayOfSeason` **1 基**(1..daysPerSeason,符合「Spring 1」式显示);`year` 从 `startYear` 起算;`minuteOfDay` 为 0 基(0..minutesPerDay-1)。`startSeason`/`startDay` 输入与之同基(`startSeason` 0 基、`startDay` 1 基)。README 复述此约定。

### 5.3 `TimeStep`(推进结果)
`{ int minutesAdvanced; int daysCrossed; int seasonsCrossed; int yearsCrossed; }`
- 计数语义(非布尔):一次 `Advance` 跨多天时如实给出跨越数量。

## 6. `TimeSystem` API 与推进机制

```cpp
class TimeSystem {
public:
    explicit TimeSystem(TimeConfig config);   // 以 config 起点初始化 totalMinutes_
    TimeStep Advance(int minutes);            // 显式整分钟推进;minutes 须 ≥ 1
    TimeStep Update(double realDeltaSeconds);  // 累积真实时间,按 step 离散吐出整分钟
    CalendarTime Now() const;                  // 派生当前日历视图
    const TimeConfig& Config() const;
private:
    TimeConfig config_;
    long long totalMinutes_;
    double realSecondAccumulator_;             // Update 的小数余量
};
```

- `Advance(minutes)`:`totalMinutes_ += minutes`,前后两个绝对计数派生日历后相减得 `TimeStep`。
- `Update(realDeltaSeconds)`:`realSecondAccumulator_ += realDeltaSeconds`;每满 `realSecondsPerStep` 取出一个步进,累计 `gameMinutesPerStep` 分钟,统一调 `Advance` 落地;余量保留。返回聚合 `TimeStep`(可能为零跨越)。

## 7. Tool API 表面

`me_toolapi` 新增对 `me_domain` 的依赖(单向:`toolapi → domain`)。`ToolContext` 增加**可选** `TimeSystem* time = nullptr` 字段——保持既有 M6/M7 的 `ToolContext` 构造与测试全部有效;时间类 Tool 在 `time == nullptr` 时返回 `PreconditionFailed`。

| Tool | 权限 | 类型 | 参数 | 结果 |
|------|------|------|------|------|
| `time.get` | `AgentAllowed` | 查询 | 无 | 当前 `CalendarTime` 的 JSON |
| `time.advance` | `Automation` | 变更(运行时态) | `{ minutes: int ≥ 1 }` | 结果 `TimeStep` + 新 `CalendarTime` |

- `time.advance` 的 `dry-run`:计算「若推进将得到的」`TimeStep`/`CalendarTime`,但**零状态改动**(不修改 `totalMinutes_`/累加器)。
- 时间推进**不经 CommandStack**(运行时状态,非场景编辑、不可 Undo);此偏离在 ADR 中显式记录。
- Schema 沿用 M6 手写子集(`type/required/properties/minimum`)。

## 8. 测试计划(纯 CPU doctest)

`tests/domain`:
- `LoadTimeConfig`:合法配置;非法(负值、`seasonNames` 数量不符、`startMinute >= minutesPerDay`)→ `nullopt`。
- `Now()` 派生:起点、当天边界(`minuteOfDay`→hour/minute)、季末天、年末季。
- `Advance`:单分钟;跨天;跨季;跨年;一次推进跨多天(`daysCrossed > 1`)。
- `Update`:累积满一步进吐分钟;不足一步进返回零跨越且保留余量;多帧累积一致性。

`tests/toolapi`(扩展):
- `time.get`:正常返回当前日历;`time == nullptr` → `PreconditionFailed`。
- `time.advance`:正常推进;schema 拒绝(缺 `minutes`/`minutes < 1`);`dry-run` 零副作用(推进后 `Now()` 不变);缺时间系统 → `PreconditionFailed`。

全部 WSL 红绿。

## 9. 数据驱动资产

新增示例时间配置 JSON(放 `assets/`,如 `assets/config/time.json`),供 sandbox / 测试加载,体现「所有参数从外部文件」原则。具体星露谷式取值(如 `minutesPerDay`、四季名)在实现计划/资产中落定,源码零硬编码数值。

## 10. 验收标准

- `me_domain` 建库,根/测试 CMake 同步,WSL 全量 doctest 绿。
- 时间四级派生与推进、`Update` 累加、`TimeStep` 计数语义均有测试覆盖。
- `time.get` / `time.advance` 经 `ToolRegistry` 流水线(白名单 + schema + dry-run + 审计)验证通过。
- `engine/domain/README.md` 记录模块职责、计数基准约定、依赖方向。
- 回写 `docs/PROGRESS.md`,新增 ADR 0006。

## 11. 开放问题 / 后续切片

- `time.set`(跳转日期)、暂停/倍速:待消费场景明确再加。
- M8.2 作物生长:消费 `TimeStep.daysCrossed` 推进生长状态机。
- M8.3 库存/物品(JSON);M8.4 NPC 日程(消费 `minuteOfDay`/`hour`)。
- 存档时 `totalMinutes_` 即时间的完整序列化表示(单一 int64),为未来存档铺路。
