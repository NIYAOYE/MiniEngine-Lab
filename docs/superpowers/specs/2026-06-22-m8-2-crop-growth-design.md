# M8.2 作物生长 — 设计文档

- **日期**:2026-06-22
- **里程碑**:M8.2(农场领域层 M8 第二切片,承接 M8.1 时间系统)
- **状态**:已确认,待写实现计划
- **前置**:M8.1 时间系统(`me_domain` 模块、`TimeSystem`/`TimeStep`、`time.get`/`time.advance` Tool)

## 1. 目标与范围

实现**浇水驱动的作物生长核心循环**:种植 → 浇水 → 跨天推进生长阶段 → 成熟 → 收获产出。逻辑全部为纯 CPU 领域代码,可在 WSL 用 doctest 红绿,经受控 Tool API 暴露。

**消费 `TimeStep.daysCrossed`**:时间系统与农场解耦,调用方读取 `time.advance` 返回的 `daysCrossed` 再喂给 `crop.advance_days`。

### 明确不做(YAGNI,留后续里程碑)

- 库存/物品系统:收获产出作为 `ToolResult` 值返回,**不存入库存**(库存是 M8.3)。
- 季节锁定、跨季枯萎、收获后重生:本里程碑作物在任意季节按浇水推进,不死亡。
- 作物精灵按阶段上屏渲染:本里程碑纯逻辑,渲染留后续切片。
- EditorController 扩展:M7 已封口,本里程碑不强制扩展。
- Windows/GPU 构建不阻塞本里程碑(纯逻辑)。

## 2. 架构与模块归属

新增逻辑全部落在 **`me_domain`**(CPU-only,沿用 `TimeSystem` 先例)。依赖图不变:

```
me_domain → me_core + nlohmann/json   (不依赖 Scene / RHI / ImGui)
```

新增两个文件对:

- **`CropDatabase`**(`include/me/domain/CropConfig.h` + `src/CropConfig.cpp`)
  数据驱动的作物表,从 JSON 加载。`LoadCropDatabase(const nlohmann::json&)` 返回
  `std::optional<CropDatabase>`(任一字段缺失/类型错/语义越界返回 `nullopt`,不抛异常),
  完全对齐 `LoadTimeConfig` 风格。

- **`FarmField`**(`include/me/domain/FarmField.h` + `src/FarmField.cpp`)
  以瓦片坐标为键的作物实例网格,持有运行时状态,提供 plant/water/harvest/advanceDays
  操作。**值语义可拷贝**(供 Tool dry-run 在副本上预演,零副作用,同 `TimeSystem`)。
  构造时注入 `CropDatabase`(值持有),用于查 stageDays / harvest 产出。

两者皆纯逻辑,可独立 doctest。

## 3. 数据模型

### 3.1 配置(`crops.json` → `CropDatabase`)

每作物一条:

```json
{
  "id": "parsnip",
  "name": "防风草",
  "stageNames": ["种子", "发芽", "成长", "成熟"],
  "stageDays": [1, 1, 1, 1],
  "harvestItemId": "parsnip",
  "yield": 1
}
```

字段语义:

- `id` — 唯一非空标识,`plant` 与查表用。
- `name` — 展示名。
- `stageNames` — 阶段名列表,0 基;**最后一个阶段即成熟/可收获阶段**。
- `stageDays[i]` — **进入下一阶段所需的“已浇水天数”**;每项 ≥ 1。最后一个阶段的值在生长上不被使用(成熟后不再前进),但仍要求存在且 ≥ 1 以保持结构一致。
- `harvestItemId` — 收获产出物品 id。
- `yield` — 单株产量,≥ 1。

**校验约束**(任一不满足 → `nullopt`):

- `stageNames.size() == stageDays.size()`,且 ≥ 1。
- `stageDays` 每项为整数且 ≥ 1。
- `yield` 为整数且 ≥ 1。
- `id` 非空;数据库内 `id` 唯一(重复 → `nullopt`)。
- `harvestItemId` 非空;`name` 非空。

`CropDatabase` 提供 `const CropConfig* Find(const std::string& id) const`(未命中返回 nullptr)。

### 3.2 运行时实例(`CropInstance`)

```cpp
struct CropInstance {
    std::string cropId;   ///< 指向 CropDatabase 的作物 id
    int stage = 0;        ///< 0 基,当前阶段索引
    int daysInStage = 0;  ///< 本阶段已累计的“已浇水天数”
    bool watered = false; ///< 今日是否已浇水
};
```

瓦片键:

```cpp
struct TileKey { int x; int y; };  // 用于 std::map 的有序键(operator< 字典序)
```

`FarmField` 内部:`std::map<TileKey, CropInstance>`(确定性遍历,doctest 易断言)。

## 4. 状态机与操作语义

```cpp
class FarmField {
public:
    explicit FarmField(CropDatabase db);

    // 域操作:返回结果状态(枚举/bool),由 Tool 层翻译为结构化 ToolResult。
    PlantResult   Plant(int x, int y, const std::string& cropId);
    bool          Water(int x, int y);          // 有作物→true(幂等);空→false
    StepResult    AdvanceDays(int n);           // n≥1;对每株重复单日推进
    HarvestResult Harvest(int x, int y);        // 成熟→产出+清空;否则失败

    const CropInstance* At(int x, int y) const; // 空→nullptr
    const std::map<TileKey, CropInstance>& Crops() const; // 只读遍历
    const CropDatabase& Database() const;

private:
    void AdvanceOneDay();                        // 内部:对所有作物推进一天
    CropDatabase db_;
    std::map<TileKey, CropInstance> crops_;
};
```

(`PlantResult` / `HarvestResult` 等为轻量结果类型,携带成功标志与失败原因 + 产出数据;具体形态在实现计划细化。)

### 4.1 操作语义

- **`Plant(x,y,cropId)`** — 瓦片空 **且** `cropId` 在数据库中存在 → 放入
  `{cropId, stage:0, daysInStage:0, watered:false}` 并成功;瓦片已占用或 cropId 不存在 → 失败(带原因)。

- **`Water(x,y)`** — 瓦片有作物 → `watered = true`(重复浇水**幂等**),成功;空瓦片 → 失败。

- **`AdvanceDays(n)`**(`n ≥ 1`)— 对每株重复 `n` 次单日推进。单日规则:
  ```
  if watered:
      daysInStage++
      watered = false
      if (stage 不是最后阶段) && (daysInStage >= stageDays[stage]):
          stage++
          daysInStage = 0
  else:
      停滞(不前进,不死亡)
  ```
  成熟阶段(`stage == 最后`)后继续浇水**不再前进**(等待收获)。

- **`Harvest(x,y)`** — 瓦片作物处于成熟阶段(`stage == stageNames.size()-1`)→
  返回 `{itemId: harvestItemId, count: yield}` 并**清空该瓦片**;未成熟或空瓦片 → 失败(带原因)。
  产出**不入库**(M8.3 接库存)。

### 4.2 设计要点:为何 `daysInStage` 计“已浇水天数”

浇水驱动模型下,未浇水的一天不计入生长(停滞),阶段推进只由有效浇水累积,符合星露谷直觉。这与“纯天数驱动”相区别——后者会忽略浇水。

## 5. ToolAPI 接线

### 5.1 `ToolContext` 扩展

与 `time` 完全对称,加可选指针(前置声明隔离,不拉入 domain 头):

```cpp
namespace me::domain { class FarmField; }

struct ToolContext {
    me::scene::Scene& scene;
    me::command::CommandStack& commands;
    ToolInvocationLog& log;
    me::domain::TimeSystem* time = nullptr;
    me::domain::FarmField*  farm = nullptr; ///< 可选:crop Tool 数据源,缺省 nullptr
};
```

M6/M7 既有构造保持有效(默认 nullptr),不破坏现有测试。

### 5.2 五个 Tool

新文件 `engine/toolapi/src/tools/CropTools.cpp`;工厂声明进 `BuiltinTools.h`;
`RegisterBuiltinTools` 追加注册。Builtin Tool 总数 **8 → 13**。

| Tool | 类别 / 权限 | params schema | 成功结果 |
|------|-------------|---------------|----------|
| `crop.get_field` | Query / **AgentAllowed** | `{}` | `{crops:[{x,y,cropId,stage,stageName,daysInStage,watered,mature}]}` |
| `crop.plant` | Mutation / **Automation** | `{tileX:int, tileY:int, cropId:string}` | 种植后实例视图 |
| `crop.water` | Mutation / **Automation** | `{tileX:int, tileY:int}` | 浇水后实例视图 |
| `crop.advance_days` | Mutation / **Automation** | `{days:int, minimum:1}` | `{advanced:n, crops:[...]}` |
| `crop.harvest` | Mutation / **EditorOnly** | `{tileX:int, tileY:int}` | `{itemId, count}` |

### 5.3 关键决策:crop 变更不经 CommandStack

作物种植/浇水/收获/推进都是**运行时游戏态**,与 M8.1 的 `time.advance` 同类。沿用
**ADR 0006 文档化例外**:「变更经 Command」是**场景编辑**约定,不适用运行时状态。
因此这些 Tool 直接经 `ctx.farm` 落地,**不可 Undo**。

### 5.4 dry-run 零副作用

沿用 `time.advance` 手法:`me::domain::FarmField preview = *ctx.farm;` 值拷贝后在副本上执行,
返回预览结果,不动真身。`crop.get_field` 只读,`dryRun` 即 `run`。

所有 Tool 入口先判 `ctx.farm == nullptr → ToolErrorCode::PreconditionFailed`,同 `ctx.time` 守卫。

### 5.5 权限梯度理由

- 查询(`get_field`)给 **AgentAllowed**。
- plant/water/advance 是自动化常规操作,给 **Automation**。
- harvest 销毁性产出(清空瓦片),收紧到 **EditorOnly**,演示三层白名单梯度(与 M6 的 `scene.destroy_entity` 同档)。

## 6. 错误处理(不抛异常)

- **配置加载**:`LoadCropDatabase` 返回 `std::optional`,字段缺失/类型错/`stageNames.size()!=stageDays.size()`/数值越界/id 重复 → `nullopt`。
- **`FarmField` 域操作**:返回带状态的结果类型,由 Tool 层翻译为结构化 `ToolResult`:
  - 瓦片已占用 / 空瓦片 / cropId 不存在 / 未成熟收获 → `PreconditionFailed`(带具体 message)。
  - `ctx.farm == nullptr` → `PreconditionFailed`。
  - 参数缺失/类型错由 Registry 的 Schema 校验**前置**拦截(`InvalidParams`),Tool 内不重复校验。
- **零魔法数字**:阶段数、产出等全部来自 `CropDatabase`;源码无裸常量。

## 7. 测试策略(全 CPU-only doctest,WSL 红绿)

预计在现有 162 基础上新增约 30–40 用例。

1. **`CropConfig` 测试** — 合法 JSON 往返;各类非法配置返回 `nullopt`(逐字段:长度不匹配、负 stageDays、yield<1、空 id、重复 id、缺字段、类型错);与 `assets/config/crops.json` 取值严格一致(同 M8.1 `ValidJson()` 先例)。
2. **`FarmField` 测试** — 核心状态机:plant→water→advanceDays 单阶段推进;多阶段跨越;**未浇水停滞**;成熟后停留(继续浇水不前进);harvest 产出 + 清空瓦片;重复浇水幂等;多天跳跃 `advanceDays(n)`;失败路径(空瓦片浇水/收获、重复种植、未成熟收获、未知 cropId)。
3. **Crop Tool 测试** — 经 `ToolRegistry::Invoke` 走完整流水线:权限白名单裁决(各角色对 5 Tool,尤其 harvest 拒绝 Agent);Schema 校验(缺 days/类型错);**dry-run 零副作用**(advance/plant/harvest 后真身不变);`ctx.farm==nullptr` 守卫;审计日志回填 invocationId。

## 8. 资产与文档

- **`assets/config/crops.json`** — 至少 2 株作物示例(如防风草 4 阶段、花椰菜多阶段),取值与 `CropConfig` 测试的 `ValidCropJson()` 严格一致(数据驱动,源码零硬编码)。
- **ADR 0007**(`docs/architecture/0007-m8-2-crop-growth.md`)— 记录决策 ①~⑥(见下)。
- **本设计 spec** — `docs/superpowers/specs/2026-06-22-m8-2-crop-growth-design.md`(本文件)。
- **`engine/domain/README.md`** — 追加 `CropDatabase`/`FarmField` 说明。
- **`docs/PROGRESS.md`** — M8 行更新 M8.2 ☑、一句话现状、下一步改 M8.3、追加 ADR 摘要行。
- **`CMakeLists.txt`** — `me_domain` 加新源文件、对应测试加入 test target。

## 9. 关键决策汇总(转 ADR 0007)

1. 作物存 `me_domain` 独立 `FarmField`(以瓦片坐标为键的网格),**非 Scene 组件** —— 沿用 `TimeSystem` 先例,纯 CPU 可 doctest,与 Scene/RHI 解耦;渲染留后续切片。
2. **浇水驱动状态机**;`daysInStage` 计“已浇水天数”而非自然天 —— 未浇水停滞(不死亡),符合星露谷核心循环。
3. 5 个 Tool;crop 变更**不经 CommandStack** —— 运行时游戏态,沿用 ADR 0006 例外;dry-run 用值拷贝副本。
4. `crop.harvest` = **EditorOnly** 权限梯度 —— 销毁性产出,与 `scene.destroy_entity` 同档,演示三层白名单。
5. 收获**产出不入库**,作为 `ToolResult` 值返回 —— 库存是 M8.3(YAGNI)。
6. 时间与农场**解耦** —— 调用方读 `TimeStep.daysCrossed` 喂 `crop.advance_days`,不在 `time.advance` 内联动。
