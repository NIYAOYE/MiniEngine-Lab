# ADR 0006：M8.1 时间系统

- 日期：2026-06-21
- 状态：已接受

## 背景
M8 农场领域层拆为多切片；M8.1 先做时间系统——作物生长/NPC 日程的共同地基。

## 决策
1. **新增 CPU-only `me_domain` 模块**（`→ me_core + nlohmann_json`），承载后续作物/库存/NPC。纯逻辑可 WSL doctest。
2. **四级日历（分钟/天/季/年），全参数 JSON 驱动**；`LoadTimeConfig` 返回 `std::optional`，不抛异常。
3. **内部用单调 `int64 totalMinutes_` 单一真相源**，日历字段即时派生；跨界计数 = floor 除法之差。否决多字段进位（易错）与 double 秒（漂移）。
4. **推进返回 `TimeStep` 践跳计数**（非布尔/非回调），消费者轮询；契合项目"显式传参、零全局状态"。
5. **开 `time.get`（AgentAllowed）/ `time.advance`（Automation）两 Tool**，经既有 Registry 流水线；`ToolContext` 加可选 `TimeSystem*`（前置声明隔离，默认 nullptr，保持 M6/M7 构造有效）。
6. **time.advance 是 Mutation 但刻意不经 CommandStack**：时间是运行时状态、Undo 无意义。dry-run 通过 TimeSystem 值拷贝在副本上推进实现零副作用。此为"变更经 Command"约定的文档化例外（该约定针对场景编辑）。

## 后果
- 时间系统单独红绿，作物（M8.2 消费 daysCrossed）/NPC（M8.4 消费 minuteOfDay）有稳定地基。
- `totalMinutes_` 即时间的完整序列化表示，为未来存档铺路。
- 未做 `time.set`/暂停/倍速（YAGNI），待消费场景明确再加。
