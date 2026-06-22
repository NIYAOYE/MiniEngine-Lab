# M8.1 时间系统 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 给 MiniEngine 加一个数据驱动的四级日历(分钟/天/季/年)时间系统,并开出 `time.get` / `time.advance` 两个受控 Tool。

**Architecture:** 新增 CPU-only `me_domain` 静态库,内部以单调 `int64 totalMinutes_` 为单一真相源、按 `TimeConfig` 即时派生日历;推进返回 `TimeStep` 践跳计数(零回调、零全局状态)。`me_toolapi` 增加对 `me_domain` 的单向依赖,`ToolContext` 增加可选 `TimeSystem*`,两个时间 Tool 经既有 `ToolRegistry` 流水线(白名单 + schema + dry-run + 审计)。

**Tech Stack:** C++17、nlohmann/json(边界格式)、doctest(单测)、CMake。

## Global Constraints

逐条来自权威设计文档与 CLAUDE.md,每个任务都隐含遵守:

- **零硬编码数值**:日历所有参数从 JSON 加载;源码不得出现裸数字常量(用具名 `constexpr` 或来自配置)。
- **不使用 C++ 异常**:可恢复失败用 `std::optional` / `ToolResult`;不变量违反用 `ME_ASSERT`(`me/core/Assert.h`)。
- **模块依赖严格单向**:`me_domain → me_core + nlohmann_json`,禁止依赖 `me_scene/me_rhi/me_renderer/me_toolapi`;`me_toolapi → me_domain` 是新增的合法单向边。
- **禁 Singleton / 禁全局可变状态**:时间状态由 `TimeSystem` 实例持有,经显式传参/`ToolContext` 注入。
- **公开 API 写 Doxygen 注释**;非显然实现写行内注释。
- **头文件不得 `using namespace std`**。
- **新增模块同步更新 CMake 与模块说明文档**。
- **计数基准(spec 已落定)**:`season` 0 基(数组下标);`dayOfSeason` 1 基;`minuteOfDay` 0 基 `[0, minutesPerDay)`;`year = startYear + 跨年数`。
- **构建/测试命令(WSL,CPU 逻辑)**:
  - 配置(新增文件后需重配):`cmake -S . -B build-wsl -DME_BUILD_TESTS=ON -DME_BUILD_GPU_TESTS=OFF`
  - 构建:`cmake --build build-wsl -j`
  - 测试:`ctest --test-dir build-wsl --output-on-failure`
  - 跑单个用例:`./build-wsl/bin/me_tests -tc="用例名"`(doctest `-tc` 按 TEST_CASE 名过滤;若 bin 路径不同,用 `find build-wsl -name me_tests`)

---

### Task 1: me_domain 模块骨架 + TimeConfig 加载与校验

建立新模块并交付第一个可测产物:从 JSON 解析 + 校验的 `TimeConfig`。模块脚手架(CMake、根/测试接线)折叠进本任务,因为 `TimeConfig` 的测试需要它们才能编译。

**Files:**
- Create: `engine/domain/CMakeLists.txt`
- Create: `engine/domain/include/me/domain/TimeConfig.h`
- Create: `engine/domain/src/TimeConfig.cpp`
- Create: `engine/domain/README.md`
- Create: `tests/domain/test_time_config.cpp`
- Modify: `CMakeLists.txt`(在 `add_subdirectory(engine/editor)` 后加 `add_subdirectory(engine/domain)`)
- Modify: `tests/CMakeLists.txt`(`me_tests` 源加 `domain/test_time_config.cpp`;`target_link_libraries` 加 `me_domain`)

**Interfaces:**
- Produces:
  - `struct me::domain::TimeConfig { int minutesPerDay, daysPerSeason, seasonsPerYear, gameMinutesPerStep; double realSecondsPerStep; int startYear, startSeason, startDay, startMinute; std::vector<std::string> seasonNames; };`
  - `std::optional<TimeConfig> me::domain::LoadTimeConfig(const nlohmann::json& j);`

- [ ] **Step 1: 写失败测试** —— `tests/domain/test_time_config.cpp`

```cpp
#include <doctest/doctest.h>

#include "me/domain/TimeConfig.h"

using me::domain::LoadTimeConfig;

namespace {
// 一份合法的最小配置(星露谷式取值,仅测试内用)。
nlohmann::json ValidJson() {
    return nlohmann::json{
        {"minutesPerDay", 1440},
        {"daysPerSeason", 28},
        {"seasonsPerYear", 4},
        {"seasonNames", {"Spring", "Summer", "Fall", "Winter"}},
        {"gameMinutesPerStep", 10},
        {"realSecondsPerStep", 7.0},
        {"startYear", 1},
        {"startSeason", 0},
        {"startDay", 1},
        {"startMinute", 360}, // 06:00
    };
}
} // namespace

TEST_CASE("TimeConfig:合法 JSON 全字段解析") {
    auto c = LoadTimeConfig(ValidJson());
    REQUIRE(c.has_value());
    CHECK(c->minutesPerDay == 1440);
    CHECK(c->daysPerSeason == 28);
    CHECK(c->seasonsPerYear == 4);
    CHECK(c->gameMinutesPerStep == 10);
    CHECK(c->realSecondsPerStep == doctest::Approx(7.0));
    CHECK(c->startYear == 1);
    CHECK(c->startSeason == 0);
    CHECK(c->startDay == 1);
    CHECK(c->startMinute == 360);
    REQUIRE(c->seasonNames.size() == 4);
    CHECK(c->seasonNames[2] == "Fall");
}

TEST_CASE("TimeConfig:非法配置一律 nullopt") {
    SUBCASE("缺字段") {
        auto j = ValidJson();
        j.erase("minutesPerDay");
        CHECK_FALSE(LoadTimeConfig(j).has_value());
    }
    SUBCASE("数值非正") {
        auto j = ValidJson();
        j["daysPerSeason"] = 0;
        CHECK_FALSE(LoadTimeConfig(j).has_value());
    }
    SUBCASE("seasonNames 数量不符 seasonsPerYear") {
        auto j = ValidJson();
        j["seasonNames"] = {"Spring", "Summer"};
        CHECK_FALSE(LoadTimeConfig(j).has_value());
    }
    SUBCASE("startMinute 越界 minutesPerDay") {
        auto j = ValidJson();
        j["startMinute"] = 1440;
        CHECK_FALSE(LoadTimeConfig(j).has_value());
    }
    SUBCASE("startSeason 越界") {
        auto j = ValidJson();
        j["startSeason"] = 4;
        CHECK_FALSE(LoadTimeConfig(j).has_value());
    }
    SUBCASE("startDay < 1") {
        auto j = ValidJson();
        j["startDay"] = 0;
        CHECK_FALSE(LoadTimeConfig(j).has_value());
    }
    SUBCASE("非对象") {
        CHECK_FALSE(LoadTimeConfig(nlohmann::json::array()).has_value());
    }
}
```

- [ ] **Step 2: 建模块脚手架(让测试能编译)**

`engine/domain/include/me/domain/TimeConfig.h`:

```cpp
#pragma once

#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace me::domain {

/**
 * @brief 数据驱动的日历参数(全部从 JSON 加载,源码零硬编码数值)。
 *
 * 计数基准:season 0 基(seasonNames 下标);startDay 1 基;
 * startMinute 0 基,范围 [0, minutesPerDay)。
 */
struct TimeConfig {
    int minutesPerDay = 0;            ///< 一天的游戏分钟数
    int daysPerSeason = 0;            ///< 一个季节的天数
    int seasonsPerYear = 0;           ///< 一年的季节数
    std::vector<std::string> seasonNames; ///< 季节名(数量须 == seasonsPerYear)
    int gameMinutesPerStep = 0;       ///< 一个推进步进推多少游戏分钟
    double realSecondsPerStep = 0.0;  ///< 现实多少秒走一个步进(Update 用)
    int startYear = 0;                ///< 初始年(year 从此起算)
    int startSeason = 0;             ///< 初始季节(0 基)
    int startDay = 1;               ///< 初始天(1 基)
    int startMinute = 0;            ///< 初始分钟(0 基)
};

/**
 * @brief 从 JSON 解析并校验 TimeConfig。
 * @return 校验通过返回配置;任一字段缺失/类型错/语义越界返回 std::nullopt(不抛异常)。
 */
std::optional<TimeConfig> LoadTimeConfig(const nlohmann::json& j);

} // namespace me::domain
```

`engine/domain/src/TimeConfig.cpp`(先放最小桩,使其编译失败于断言而非链接):

```cpp
#include "me/domain/TimeConfig.h"

namespace me::domain {

std::optional<TimeConfig> LoadTimeConfig(const nlohmann::json&) {
    return std::nullopt; // 桩:Step 4 实现
}

} // namespace me::domain
```

`engine/domain/CMakeLists.txt`:

```cmake
add_library(me_domain STATIC
    src/TimeConfig.cpp
)
target_include_directories(me_domain PUBLIC include)
target_compile_features(me_domain PUBLIC cxx_std_17)
# 单向依赖:domain → core。nlohmann/json 出现在公开头(TimeConfig.h)→ PUBLIC。
# 严禁依赖 scene / command / toolapi / rhi / renderer。
target_link_libraries(me_domain PUBLIC me_core nlohmann_json::nlohmann_json)
```

`engine/domain/README.md`:

```markdown
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
```

根 `CMakeLists.txt`——在 `add_subdirectory(engine/editor)` 一行之后插入:

```cmake
add_subdirectory(engine/domain)
```

`tests/CMakeLists.txt`——在 `editor/test_editor_controller.cpp` 行后(`)` 之前)加入源文件:

```cmake
    domain/test_time_config.cpp
```

并在 `target_link_libraries(me_tests PRIVATE ...)` 末尾追加 `me_domain`:

```cmake
target_link_libraries(me_tests PRIVATE doctest::doctest me_core me_platform me_rhi_cpu me_assets me_scene me_command me_toolapi me_editor me_domain)
```

- [ ] **Step 3: 配置 + 构建 + 跑测试,确认失败**

Run:
```bash
cmake -S . -B build-wsl -DME_BUILD_TESTS=ON -DME_BUILD_GPU_TESTS=OFF
cmake --build build-wsl -j
ctest --test-dir build-wsl --output-on-failure
```
Expected: 编译链接通过,`TimeConfig:合法 JSON 全字段解析` FAIL(桩返回 nullopt,`REQUIRE(c.has_value())` 失败)。

- [ ] **Step 4: 实现 LoadTimeConfig**

替换 `engine/domain/src/TimeConfig.cpp` 全文:

```cpp
#include "me/domain/TimeConfig.h"

namespace me::domain {
namespace {

// 读一个必需的整数字段;缺失或非整数返回 false。
bool ReadInt(const nlohmann::json& j, const char* key, int& out) {
    if (!j.contains(key) || !j[key].is_number_integer()) return false;
    out = j[key].get<int>();
    return true;
}

} // namespace

std::optional<TimeConfig> LoadTimeConfig(const nlohmann::json& j) {
    if (!j.is_object()) return std::nullopt;

    TimeConfig c;
    if (!ReadInt(j, "minutesPerDay", c.minutesPerDay)) return std::nullopt;
    if (!ReadInt(j, "daysPerSeason", c.daysPerSeason)) return std::nullopt;
    if (!ReadInt(j, "seasonsPerYear", c.seasonsPerYear)) return std::nullopt;
    if (!ReadInt(j, "gameMinutesPerStep", c.gameMinutesPerStep)) return std::nullopt;
    if (!ReadInt(j, "startYear", c.startYear)) return std::nullopt;
    if (!ReadInt(j, "startSeason", c.startSeason)) return std::nullopt;
    if (!ReadInt(j, "startDay", c.startDay)) return std::nullopt;
    if (!ReadInt(j, "startMinute", c.startMinute)) return std::nullopt;

    if (!j.contains("realSecondsPerStep") || !j["realSecondsPerStep"].is_number())
        return std::nullopt;
    c.realSecondsPerStep = j["realSecondsPerStep"].get<double>();

    if (!j.contains("seasonNames") || !j["seasonNames"].is_array()) return std::nullopt;
    for (const auto& n : j["seasonNames"]) {
        if (!n.is_string()) return std::nullopt;
        c.seasonNames.push_back(n.get<std::string>());
    }

    // —— 语义校验(不变量,违反即非法配置)——
    if (c.minutesPerDay <= 0 || c.daysPerSeason <= 0 || c.seasonsPerYear <= 0)
        return std::nullopt;
    if (c.gameMinutesPerStep <= 0 || c.realSecondsPerStep <= 0.0) return std::nullopt;
    if (static_cast<int>(c.seasonNames.size()) != c.seasonsPerYear) return std::nullopt;
    if (c.startSeason < 0 || c.startSeason >= c.seasonsPerYear) return std::nullopt;
    if (c.startDay < 1 || c.startDay > c.daysPerSeason) return std::nullopt;
    if (c.startMinute < 0 || c.startMinute >= c.minutesPerDay) return std::nullopt;

    return c;
}

} // namespace me::domain
```

- [ ] **Step 5: 构建 + 跑测试,确认通过**

Run:
```bash
cmake --build build-wsl -j && ctest --test-dir build-wsl --output-on-failure
```
Expected: `TimeConfig:*` 全 PASS,其余既有用例不回归。

- [ ] **Step 6: 提交**

```bash
git add engine/domain CMakeLists.txt tests/CMakeLists.txt tests/domain/test_time_config.cpp
git commit -m "feat(domain): M8.1 me_domain 模块 + TimeConfig 数据驱动加载与校验"
```

---

### Task 2: TimeSystem —— Now() 派生 + Advance() 践跳

交付时钟核心:从 `TimeConfig` 起点初始化、`Now()` 派生四级日历、`Advance(minutes)` 推进并返回跨界计数。

**Files:**
- Create: `engine/domain/include/me/domain/TimeSystem.h`
- Create: `engine/domain/src/TimeSystem.cpp`
- Create: `tests/domain/test_time_system.cpp`
- Modify: `engine/domain/CMakeLists.txt`(源加 `src/TimeSystem.cpp`)
- Modify: `tests/CMakeLists.txt`(`me_tests` 源加 `domain/test_time_system.cpp`)

**Interfaces:**
- Consumes: `me::domain::TimeConfig`(Task 1)。
- Produces:
  - `struct me::domain::CalendarTime { int year, season; std::string seasonName; int dayOfSeason, minuteOfDay, hour, minute; };`
  - `struct me::domain::TimeStep { int minutesAdvanced, daysCrossed, seasonsCrossed, yearsCrossed; };`
  - `class me::domain::TimeSystem { explicit TimeSystem(TimeConfig); TimeStep Advance(int minutes); TimeStep Update(double realDeltaSeconds); CalendarTime Now() const; const TimeConfig& Config() const; };`(本任务实现除 `Update` 外的全部;`Update` 在 Task 3 实现)

- [ ] **Step 1: 写失败测试** —— `tests/domain/test_time_system.cpp`

```cpp
#include <doctest/doctest.h>

#include "me/domain/TimeSystem.h"

using namespace me::domain;

namespace {
// 简洁取值,便于手算边界:一天 100 分钟、一季 3 天、一年 2 季。
TimeConfig SmallConfig() {
    TimeConfig c;
    c.minutesPerDay = 100;
    c.daysPerSeason = 3;
    c.seasonsPerYear = 2;
    c.seasonNames = {"Spring", "Summer"};
    c.gameMinutesPerStep = 10;
    c.realSecondsPerStep = 1.0;
    c.startYear = 1;
    c.startSeason = 0;
    c.startDay = 1;
    c.startMinute = 0;
    return c;
}
} // namespace

TEST_CASE("TimeSystem:起点 Now() 派生") {
    TimeSystem ts(SmallConfig());
    auto n = ts.Now();
    CHECK(n.year == 1);
    CHECK(n.season == 0);
    CHECK(n.seasonName == "Spring");
    CHECK(n.dayOfSeason == 1);
    CHECK(n.minuteOfDay == 0);
    CHECK(n.hour == 0);
    CHECK(n.minute == 0);
}

TEST_CASE("TimeSystem:Now() hour/minute 由 minuteOfDay 派生") {
    auto cfg = SmallConfig();
    cfg.minutesPerDay = 1440;
    cfg.startMinute = 125; // 02:05
    TimeSystem ts(cfg);
    auto n = ts.Now();
    CHECK(n.minuteOfDay == 125);
    CHECK(n.hour == 2);
    CHECK(n.minute == 5);
}

TEST_CASE("TimeSystem:Advance 单分钟不跨界") {
    TimeSystem ts(SmallConfig());
    auto s = ts.Advance(1);
    CHECK(s.minutesAdvanced == 1);
    CHECK(s.daysCrossed == 0);
    CHECK(s.seasonsCrossed == 0);
    CHECK(s.yearsCrossed == 0);
    CHECK(ts.Now().minuteOfDay == 1);
}

TEST_CASE("TimeSystem:Advance 跨天") {
    TimeSystem ts(SmallConfig()); // 一天 100 分钟
    auto s = ts.Advance(100);
    CHECK(s.daysCrossed == 1);
    CHECK(s.seasonsCrossed == 0);
    auto n = ts.Now();
    CHECK(n.dayOfSeason == 2);
    CHECK(n.minuteOfDay == 0);
}

TEST_CASE("TimeSystem:Advance 跨季") {
    TimeSystem ts(SmallConfig()); // 一季 = 3*100 = 300 分钟
    auto s = ts.Advance(300);
    CHECK(s.daysCrossed == 3);
    CHECK(s.seasonsCrossed == 1);
    auto n = ts.Now();
    CHECK(n.season == 1);
    CHECK(n.seasonName == "Summer");
    CHECK(n.dayOfSeason == 1);
}

TEST_CASE("TimeSystem:Advance 跨年") {
    TimeSystem ts(SmallConfig()); // 一年 = 2*300 = 600 分钟
    auto s = ts.Advance(600);
    CHECK(s.yearsCrossed == 1);
    CHECK(s.seasonsCrossed == 2);
    auto n = ts.Now();
    CHECK(n.year == 2);
    CHECK(n.season == 0);
    CHECK(n.dayOfSeason == 1);
}

TEST_CASE("TimeSystem:Advance 一次跨多天计数正确") {
    TimeSystem ts(SmallConfig());
    auto s = ts.Advance(250); // 2.5 天
    CHECK(s.daysCrossed == 2);
    CHECK(ts.Now().dayOfSeason == 3);
    CHECK(ts.Now().minuteOfDay == 50);
}

TEST_CASE("TimeSystem:Advance 从非零起点正确累加跨界") {
    auto cfg = SmallConfig();
    cfg.startMinute = 50; // 距跨天还差 50 分钟
    TimeSystem ts(cfg);
    auto s = ts.Advance(60);
    CHECK(s.daysCrossed == 1); // 50→110 跨过 100 边界一次
}
```

- [ ] **Step 2: 建头 + 桩,构建确认失败**

`engine/domain/include/me/domain/TimeSystem.h`:

```cpp
#pragma once

#include <string>

#include "me/domain/TimeConfig.h"

namespace me::domain {

/// @brief 当前时间的派生视图(由 totalMinutes_ 按 config 算出)。
struct CalendarTime {
    int year = 0;        ///< startYear + 跨年数
    int season = 0;      ///< 0 基索引
    std::string seasonName;
    int dayOfSeason = 1; ///< 1 基
    int minuteOfDay = 0; ///< 0 基,[0, minutesPerDay)
    int hour = 0;        ///< minuteOfDay / 60
    int minute = 0;      ///< minuteOfDay % 60
};

/// @brief 一次推进跨越的边界计数(非布尔,保留多天跳跃信息)。
struct TimeStep {
    int minutesAdvanced = 0;
    int daysCrossed = 0;
    int seasonsCrossed = 0;
    int yearsCrossed = 0;
};

/**
 * @brief 四级日历时钟(分钟/天/季/年)。
 *
 * 内部以单调 totalMinutes_(自纪元起总分钟)为单一真相源,日历字段即时派生。
 * 值语义可拷贝:Tool 的 dry-run 通过拷贝实例在副本上推进实现零副作用。
 * 时间是运行时状态,不进 Command/Undo(见 ADR 0006)。
 */
class TimeSystem {
public:
    /// @brief 以 config 的 start* 字段为起点初始化时钟。
    explicit TimeSystem(TimeConfig config);

    /// @brief 显式推进整分钟(minutes 须 ≥ 1),返回跨界计数。
    TimeStep Advance(int minutes);

    /// @brief 按真实经过秒数累积推进(Task 3 实现);返回聚合跨界计数。
    TimeStep Update(double realDeltaSeconds);

    /// @brief 当前日历视图。
    CalendarTime Now() const;

    /// @brief 只读访问配置。
    const TimeConfig& Config() const { return config_; }

private:
    /// @brief 把一个绝对总分钟数派生为日历视图。
    CalendarTime CalendarAt(long long totalMinutes) const;

    TimeConfig config_;
    long long totalMinutes_ = 0;        ///< 单一真相源
    double realSecondAccumulator_ = 0.0;///< Update 的小数余量
};

} // namespace me::domain
```

`engine/domain/src/TimeSystem.cpp`(桩):

```cpp
#include "me/domain/TimeSystem.h"

namespace me::domain {

TimeSystem::TimeSystem(TimeConfig config) : config_(std::move(config)) {}
TimeStep TimeSystem::Advance(int) { return {}; }
TimeStep TimeSystem::Update(double) { return {}; }
CalendarTime TimeSystem::Now() const { return {}; }
CalendarTime TimeSystem::CalendarAt(long long) const { return {}; }

} // namespace me::domain
```

在 `engine/domain/CMakeLists.txt` 的 `add_library` 源列表加一行 `src/TimeSystem.cpp`:

```cmake
add_library(me_domain STATIC
    src/TimeConfig.cpp
    src/TimeSystem.cpp
)
```

在 `tests/CMakeLists.txt` 的 `me_tests` 源列表 `domain/test_time_config.cpp` 后加一行:

```cmake
    domain/test_time_system.cpp
```

Run:
```bash
cmake -S . -B build-wsl -DME_BUILD_TESTS=ON -DME_BUILD_GPU_TESTS=OFF
cmake --build build-wsl -j && ctest --test-dir build-wsl --output-on-failure
```
Expected: 编译链接通过,`TimeSystem:*` 多数 FAIL(桩返回零值)。

- [ ] **Step 3: 实现 TimeSystem(替换 .cpp 全文)**

```cpp
#include "me/domain/TimeSystem.h"

#include <utility>

#include "me/core/Assert.h"

namespace me::domain {
namespace {

// 计算 [0, ...) 两个绝对分钟数落在不同 unit 桶时的跨界次数(floor 除法之差)。
int CrossCount(long long before, long long after, long long unit) {
    return static_cast<int>(after / unit - before / unit);
}

} // namespace

TimeSystem::TimeSystem(TimeConfig config) : config_(std::move(config)) {
    const long long mpd = config_.minutesPerDay;
    const long long mps = mpd * config_.daysPerSeason;
    // 纪元 = (startYear, season 0, day 1, minute 0);start* 给出纪元内偏移。
    totalMinutes_ = static_cast<long long>(config_.startSeason) * mps
                  + static_cast<long long>(config_.startDay - 1) * mpd
                  + config_.startMinute;
}

TimeStep TimeSystem::Advance(int minutes) {
    ME_ASSERT(minutes >= 1); // 不变量:推进必为正整数分钟
    const long long mpd = config_.minutesPerDay;
    const long long mps = mpd * config_.daysPerSeason;
    const long long mpy = mps * config_.seasonsPerYear;

    const long long before = totalMinutes_;
    const long long after = before + minutes;

    TimeStep step;
    step.minutesAdvanced = minutes;
    step.daysCrossed = CrossCount(before, after, mpd);
    step.seasonsCrossed = CrossCount(before, after, mps);
    step.yearsCrossed = CrossCount(before, after, mpy);

    totalMinutes_ = after;
    return step;
}

TimeStep TimeSystem::Update(double) {
    return {}; // Task 3 实现
}

CalendarTime TimeSystem::CalendarAt(long long total) const {
    const long long mpd = config_.minutesPerDay;
    const long long mps = mpd * config_.daysPerSeason;
    const long long mpy = mps * config_.seasonsPerYear;

    const long long years = total / mpy;
    long long rem = total % mpy;
    const long long season = rem / mps;
    rem = rem % mps;
    const long long dayIdx = rem / mpd;
    const long long minOfDay = rem % mpd;

    CalendarTime t;
    t.year = config_.startYear + static_cast<int>(years);
    t.season = static_cast<int>(season);
    t.seasonName = config_.seasonNames[static_cast<std::size_t>(season)];
    t.dayOfSeason = static_cast<int>(dayIdx) + 1;
    t.minuteOfDay = static_cast<int>(minOfDay);
    t.hour = t.minuteOfDay / 60;
    t.minute = t.minuteOfDay % 60;
    return t;
}

CalendarTime TimeSystem::Now() const { return CalendarAt(totalMinutes_); }

} // namespace me::domain
```

- [ ] **Step 4: 构建 + 跑测试,确认通过**

Run:
```bash
cmake --build build-wsl -j && ctest --test-dir build-wsl --output-on-failure
```
Expected: `TimeSystem:*`(除 Update 相关,本任务未测)全 PASS,无回归。

- [ ] **Step 5: 提交**

```bash
git add engine/domain tests/CMakeLists.txt tests/domain/test_time_system.cpp
git commit -m "feat(domain): TimeSystem Now() 四级派生 + Advance() 践跳计数"
```

---

### Task 3: TimeSystem —— Update() 真实秒累加器

补齐运行时入口:按真实经过秒数累积,满 `realSecondsPerStep` 吐出 `gameMinutesPerStep` 分钟,余量保留。

**Files:**
- Modify: `engine/domain/src/TimeSystem.cpp:TimeSystem::Update`
- Modify: `tests/domain/test_time_system.cpp`(追加用例)

**Interfaces:**
- Consumes: `TimeSystem::Advance`、`config_.realSecondsPerStep`、`config_.gameMinutesPerStep`(本模块内)。
- Produces: `TimeStep TimeSystem::Update(double realDeltaSeconds)` 行为契约(见测试)。

- [ ] **Step 1: 追加失败测试**(贴到 `tests/domain/test_time_system.cpp` 末尾)

```cpp
TEST_CASE("TimeSystem:Update 累满一步进吐分钟") {
    auto cfg = SmallConfig();
    cfg.realSecondsPerStep = 1.0;
    cfg.gameMinutesPerStep = 10;
    TimeSystem ts(cfg);
    auto s = ts.Update(1.0); // 恰好一个步进
    CHECK(s.minutesAdvanced == 10);
    CHECK(ts.Now().minuteOfDay == 10);
}

TEST_CASE("TimeSystem:Update 不足一步进零跨越且保留余量") {
    auto cfg = SmallConfig();
    cfg.realSecondsPerStep = 1.0;
    cfg.gameMinutesPerStep = 10;
    TimeSystem ts(cfg);
    auto s0 = ts.Update(0.4);
    CHECK(s0.minutesAdvanced == 0);
    CHECK(ts.Now().minuteOfDay == 0);
    auto s1 = ts.Update(0.7); // 0.4 + 0.7 = 1.1 → 跨一个步进,余 0.1
    CHECK(s1.minutesAdvanced == 10);
    CHECK(ts.Now().minuteOfDay == 10);
}

TEST_CASE("TimeSystem:Update 一帧跨多步进聚合") {
    auto cfg = SmallConfig();
    cfg.realSecondsPerStep = 1.0;
    cfg.gameMinutesPerStep = 10;
    TimeSystem ts(cfg);
    auto s = ts.Update(2.5); // 2 个完整步进
    CHECK(s.minutesAdvanced == 20);
    CHECK(ts.Now().minuteOfDay == 20);
}
```

- [ ] **Step 2: 构建 + 跑,确认新用例失败**

Run:
```bash
cmake --build build-wsl -j && ./build-wsl/bin/me_tests -tc="TimeSystem:Update*"
```
Expected: 三个 `Update` 用例 FAIL(桩返回零),Task 2 用例仍 PASS。

- [ ] **Step 3: 实现 Update(替换 `TimeSystem::Update` 函数体)**

```cpp
TimeStep TimeSystem::Update(double realDeltaSeconds) {
    if (realDeltaSeconds > 0.0) realSecondAccumulator_ += realDeltaSeconds;

    int minutesToAdvance = 0;
    while (realSecondAccumulator_ >= config_.realSecondsPerStep) {
        realSecondAccumulator_ -= config_.realSecondsPerStep;
        minutesToAdvance += config_.gameMinutesPerStep;
    }
    if (minutesToAdvance == 0) return {}; // 不足一步进:零跨越,余量已保留
    return Advance(minutesToAdvance);
}
```

- [ ] **Step 4: 构建 + 跑全量测试,确认通过**

Run:
```bash
cmake --build build-wsl -j && ctest --test-dir build-wsl --output-on-failure
```
Expected: `TimeSystem:Update*` 全 PASS,无回归。

- [ ] **Step 5: 提交**

```bash
git add engine/domain/src/TimeSystem.cpp tests/domain/test_time_system.cpp
git commit -m "feat(domain): TimeSystem::Update 真实秒累加器(步进离散吐分钟)"
```

---

### Task 4: ToolContext 接入 TimeSystem + time.get Tool

`me_toolapi` 新增对 `me_domain` 的单向依赖,`ToolContext` 增可选 `TimeSystem*`,实现并注册只读 `time.get`。

**Files:**
- Modify: `engine/toolapi/include/me/toolapi/ToolContext.h`
- Create: `engine/toolapi/src/tools/TimeTools.cpp`
- Modify: `engine/toolapi/include/me/toolapi/tools/BuiltinTools.h`
- Modify: `engine/toolapi/src/tools/BuiltinTools.cpp`
- Modify: `engine/toolapi/CMakeLists.txt`(源加 `src/tools/TimeTools.cpp`;link 加 `me_domain`)
- Modify: `tests/toolapi/test_builtin_integration.cpp`(6 → 8)
- Create: `tests/toolapi/test_time_tools.cpp`
- Modify: `tests/CMakeLists.txt`(`me_tests` 源加 `toolapi/test_time_tools.cpp`)

**Interfaces:**
- Consumes: `me::domain::TimeSystem`(Task 2/3);`ITool`/`ToolResult`/`ToolContext`/`ToolRegistry`(M6)。
- Produces:
  - `ToolContext` 新字段 `me::domain::TimeSystem* time = nullptr;`
  - `std::unique_ptr<ITool> me::toolapi::MakeTimeGetTool();`
  - `nlohmann::json me::toolapi::CalendarToJson(const me::domain::CalendarTime&);`(供时间 Tool 复用,声明在 BuiltinTools.h)

- [ ] **Step 1: 写失败测试** —— `tests/toolapi/test_time_tools.cpp`

```cpp
#include <doctest/doctest.h>

#include "me/command/CommandStack.h"
#include "me/domain/TimeSystem.h"
#include "me/scene/Scene.h"
#include "me/toolapi/ToolContext.h"
#include "me/toolapi/ToolRegistry.h"
#include "me/toolapi/tools/BuiltinTools.h"

using namespace me::toolapi;

namespace {
me::domain::TimeConfig SmallConfig() {
    me::domain::TimeConfig c;
    c.minutesPerDay = 100;
    c.daysPerSeason = 3;
    c.seasonsPerYear = 2;
    c.seasonNames = {"Spring", "Summer"};
    c.gameMinutesPerStep = 10;
    c.realSecondsPerStep = 1.0;
    c.startYear = 1;
    c.startSeason = 0;
    c.startDay = 1;
    c.startMinute = 0;
    return c;
}
} // namespace

TEST_CASE("TimeTools:time.get 返回当前日历") {
    ToolRegistry reg;
    reg.Register(MakeTimeGetTool());
    me::scene::Scene scene;
    me::command::CommandStack stack;
    ToolInvocationLog log;
    me::domain::TimeSystem ts(SmallConfig());
    ToolContext ctx{scene, stack, log, &ts};

    auto r = reg.Invoke("time.get", nlohmann::json::object(), CallerRole::Agent, ctx);
    REQUIRE(r.ok);
    CHECK(r.data["year"] == 1);
    CHECK(r.data["season"] == 0);
    CHECK(r.data["seasonName"] == "Spring");
    CHECK(r.data["dayOfSeason"] == 1);
    CHECK(r.data["minuteOfDay"] == 0);
}

TEST_CASE("TimeTools:time.get 无时间系统 PreconditionFailed") {
    ToolRegistry reg;
    reg.Register(MakeTimeGetTool());
    me::scene::Scene scene;
    me::command::CommandStack stack;
    ToolInvocationLog log;
    ToolContext ctx{scene, stack, log}; // time 默认 nullptr

    auto r = reg.Invoke("time.get", nlohmann::json::object(), CallerRole::Agent, ctx);
    CHECK_FALSE(r.ok);
    CHECK(r.code == ToolErrorCode::PreconditionFailed);
}
```

- [ ] **Step 2: 改 ToolContext + BuiltinTools.h + 建 TimeTools 桩 + CMake,构建确认失败**

`engine/toolapi/include/me/toolapi/ToolContext.h` —— 在文件 `namespace me::toolapi {` 之前加前置声明,并给结构体加字段:

```cpp
#pragma once

#include "me/command/CommandStack.h"
#include "me/scene/Scene.h"
#include "me/toolapi/ToolInvocation.h"

namespace me::domain { class TimeSystem; } // 前置声明:仅按指针使用,避免拉入 domain 头

namespace me::toolapi {

/**
 * @brief 注入给 Tool 的受控门面:Tool 只能经此访问被授权的子系统。
 *
 * 不持有全局指针(契合禁 Singleton / 禁全局可变状态)。变更型 Tool 经 commands
 * 落地以获得 Undo;查询型 Tool 直接读 scene;log.read 读 log;时间 Tool 读 time。
 */
struct ToolContext {
    me::scene::Scene& scene;            ///< 受控场景访问
    me::command::CommandStack& commands;///< 变更型 Tool 的唯一落地通道
    ToolInvocationLog& log;             ///< 审计日志(log.read 数据源)
    me::domain::TimeSystem* time = nullptr; ///< 可选:时间 Tool 数据源,缺省 nullptr
};

} // namespace me::toolapi
```

`engine/toolapi/include/me/toolapi/tools/BuiltinTools.h` —— 两处改动。

(a)在文件顶部 includes 之后、`namespace me::toolapi {` **之前**,加 domain 前置声明(放全局作用域,struct 仅按引用使用):

```cpp
namespace me::domain { struct CalendarTime; }
```

(b)在 `me::toolapi` 命名空间内,`MakeSetTransformTool();` 行之后加:

```cpp
/// @brief CalendarTime → JSON { year, season, seasonName, dayOfSeason, minuteOfDay, hour, minute }。
nlohmann::json CalendarToJson(const me::domain::CalendarTime& c);

// —— 时间型 Tool 工厂(M8.1)——
std::unique_ptr<ITool> MakeTimeGetTool();     ///< time.get
std::unique_ptr<ITool> MakeTimeAdvanceTool(); ///< time.advance(Task 5)
```

`engine/toolapi/src/tools/TimeTools.cpp`(桩,仅 time.get 返回错误占位,使其编译):

```cpp
#include "me/domain/TimeSystem.h"
#include "me/toolapi/ToolContext.h"
#include "me/toolapi/tools/BuiltinTools.h"

namespace me::toolapi {

nlohmann::json CalendarToJson(const me::domain::CalendarTime& c) {
    return nlohmann::json{
        {"year", c.year},           {"season", c.season},
        {"seasonName", c.seasonName},{"dayOfSeason", c.dayOfSeason},
        {"minuteOfDay", c.minuteOfDay},{"hour", c.hour},{"minute", c.minute},
    };
}

std::unique_ptr<ITool> MakeTimeGetTool() { return nullptr; } // 桩:Step 4 实现

} // namespace me::toolapi
```

`engine/toolapi/CMakeLists.txt` —— 源列表加 `src/tools/TimeTools.cpp`,link 行加 `me_domain`:

```cmake
add_library(me_toolapi STATIC
    src/ToolResult.cpp
    src/SchemaValidator.cpp
    src/ToolInvocation.cpp
    src/ToolRegistry.cpp
    src/tools/QueryTools.cpp
    src/tools/MutationTools.cpp
    src/tools/BuiltinTools.cpp
    src/tools/TimeTools.cpp
)
target_include_directories(me_toolapi PUBLIC include)
target_compile_features(me_toolapi PUBLIC cxx_std_17)
# 单向依赖:toolapi → command → scene → core,以及 toolapi → domain(时间 Tool)。
target_link_libraries(me_toolapi PUBLIC me_command me_scene me_core me_domain nlohmann_json::nlohmann_json)
```

`tests/CMakeLists.txt` —— `me_tests` 源列表加一行(放在 `toolapi/test_builtin_integration.cpp` 后):

```cmake
    toolapi/test_time_tools.cpp
```

Run:
```bash
cmake -S . -B build-wsl -DME_BUILD_TESTS=ON -DME_BUILD_GPU_TESTS=OFF
cmake --build build-wsl -j && ./build-wsl/bin/me_tests -tc="TimeTools:*"
```
Expected: 编译链接通过;`TimeTools:time.get*` FAIL(`MakeTimeGetTool` 返回 nullptr,`Register` 失败 → `Invoke` 得 UnknownTool,`r.ok` 为 false 且非预期 code)。

- [ ] **Step 3: (无独立步骤,合入 Step 4)**

- [ ] **Step 4: 实现 time.get(替换 TimeTools.cpp 中 `MakeTimeGetTool` 桩)**

把 `std::unique_ptr<ITool> MakeTimeGetTool() { return nullptr; }` 替换为匿名命名空间类 + 工厂:

```cpp
namespace {

// time.get:返回当前日历视图(只读)。
class TimeGetTool final : public ITool {
public:
    std::string name() const override { return "time.get"; }
    ToolCategory category() const override { return ToolCategory::Query; }
    Permission permission() const override { return Permission::AgentAllowed; }
    nlohmann::json paramsSchema() const override {
        return {{"type", "object"}, {"properties", nlohmann::json::object()}};
    }
    ToolResult dryRun(ToolContext& ctx, const nlohmann::json& p) const override {
        return run(ctx, p); // 只读:预览即结果
    }
    ToolResult run(ToolContext& ctx, const nlohmann::json&) const override {
        if (ctx.time == nullptr)
            return ToolResult::Error(ToolErrorCode::PreconditionFailed,
                                     "no time system wired into ToolContext");
        return ToolResult::Success(CalendarToJson(ctx.time->Now()));
    }
};

} // namespace

std::unique_ptr<ITool> MakeTimeGetTool() { return std::make_unique<TimeGetTool>(); }
```

> 该匿名命名空间块要放在 `CalendarToJson` 定义之后、文件末尾 `MakeTimeGetTool` 工厂定义之前。确保 `#include <memory>` 可用——`BuiltinTools.h` 已 include `<memory>`,传递可见。

- [ ] **Step 5: 把 time.get 注册进 RegisterBuiltinTools 并修整合测试**

`engine/toolapi/src/tools/BuiltinTools.cpp` —— 在 `RegisterBuiltinTools` 函数体末尾(变更型注册之后)**只加这一行**(`MakeTimeAdvanceTool` 要到 Task 5 才有定义,提前注册会链接报错):

```cpp
    // 时间型(M8.1)
    registry.Register(MakeTimeGetTool());
```

> 故本任务整合测试期望 7;Task 5 再补 `MakeTimeAdvanceTool()` 注册并到 8。

`tests/toolapi/test_builtin_integration.cpp` —— 把 `CHECK(reg.Size() == 6);` 改为 `CHECK(reg.Size() == 7);`,标题与注释同步:

```cpp
TEST_CASE("Integration:RegisterBuiltinTools 注册全部 7 个 Tool") {
    ToolRegistry reg;
    RegisterBuiltinTools(reg);
    CHECK(reg.Size() == 7);
    auto names = reg.ListNames();
    // 字典序前两个不变(time.* 排在 scene.* 之后)
    CHECK(names[0] == "entity.set_transform");
    CHECK(names[1] == "log.read");
}
```

- [ ] **Step 6: 构建 + 跑全量测试,确认通过**

Run:
```bash
cmake --build build-wsl -j && ctest --test-dir build-wsl --output-on-failure
```
Expected: `TimeTools:time.get*` PASS;整合测试 7 PASS;无回归。

- [ ] **Step 7: 提交**

```bash
git add engine/toolapi tests/CMakeLists.txt tests/toolapi/test_time_tools.cpp tests/toolapi/test_builtin_integration.cpp
git commit -m "feat(toolapi): ToolContext 接入可选 TimeSystem + time.get 只读 Tool"
```

---

### Task 5: time.advance Tool(变更运行时态,dry-run 零副作用)

实现并注册 `time.advance`:经 schema 校验推进 N 分钟;dry-run 在值拷贝副本上推进以保证零副作用;不经 CommandStack(运行时状态,文档化例外)。

**Files:**
- Modify: `engine/toolapi/src/tools/TimeTools.cpp`(`MakeTimeAdvanceTool` 桩 → 实现 + TimeStepToJson)
- Modify: `engine/toolapi/src/tools/BuiltinTools.cpp`(注册 `MakeTimeAdvanceTool`)
- Modify: `tests/toolapi/test_builtin_integration.cpp`(7 → 8)
- Modify: `tests/toolapi/test_time_tools.cpp`(追加用例)

**Interfaces:**
- Consumes: `TimeSystem`(可拷贝值语义)、`CalendarToJson`(Task 4)、schema 流水线(M6)。
- Produces: `std::unique_ptr<ITool> me::toolapi::MakeTimeAdvanceTool();`(已在 BuiltinTools.h 声明)。time.advance 成功结果形如 `data = { step:{minutesAdvanced,daysCrossed,seasonsCrossed,yearsCrossed}, time:{...CalendarTime} }`。

- [ ] **Step 1: 追加失败测试**(贴到 `tests/toolapi/test_time_tools.cpp` 末尾)

```cpp
TEST_CASE("TimeTools:time.advance 推进改变当前时间") {
    ToolRegistry reg;
    reg.Register(MakeTimeAdvanceTool());
    me::scene::Scene scene;
    me::command::CommandStack stack;
    ToolInvocationLog log;
    me::domain::TimeSystem ts(SmallConfig()); // 一天 100 分钟
    ToolContext ctx{scene, stack, log, &ts};

    auto r = reg.Invoke("time.advance", {{"minutes", 120}}, CallerRole::Automation, ctx);
    REQUIRE(r.ok);
    CHECK(r.data["step"]["minutesAdvanced"] == 120);
    CHECK(r.data["step"]["daysCrossed"] == 1);
    CHECK(r.data["time"]["dayOfSeason"] == 2);
    CHECK(r.data["time"]["minuteOfDay"] == 20);
    CHECK(ts.Now().minuteOfDay == 20); // 真实状态已推进
}

TEST_CASE("TimeTools:time.advance dry-run 零副作用") {
    ToolRegistry reg;
    reg.Register(MakeTimeAdvanceTool());
    me::scene::Scene scene;
    me::command::CommandStack stack;
    ToolInvocationLog log;
    me::domain::TimeSystem ts(SmallConfig());
    ToolContext ctx{scene, stack, log, &ts};

    auto r = reg.Invoke("time.advance", {{"minutes", 120}}, CallerRole::Automation, ctx,
                        /*dryRun=*/true);
    REQUIRE(r.ok);
    CHECK(r.data["step"]["daysCrossed"] == 1);     // 预览给出将会发生什么
    CHECK(ts.Now().minuteOfDay == 0);               // 真实状态未变
    CHECK(ts.Now().dayOfSeason == 1);
}

TEST_CASE("TimeTools:time.advance schema 拒绝") {
    ToolRegistry reg;
    reg.Register(MakeTimeAdvanceTool());
    me::scene::Scene scene;
    me::command::CommandStack stack;
    ToolInvocationLog log;
    me::domain::TimeSystem ts(SmallConfig());
    ToolContext ctx{scene, stack, log, &ts};

    SUBCASE("缺 minutes") {
        auto r = reg.Invoke("time.advance", nlohmann::json::object(),
                            CallerRole::Automation, ctx);
        CHECK_FALSE(r.ok);
        CHECK(r.code == ToolErrorCode::InvalidParams);
    }
    SUBCASE("minutes < 1") {
        auto r = reg.Invoke("time.advance", {{"minutes", 0}}, CallerRole::Automation, ctx);
        CHECK_FALSE(r.ok);
        CHECK(r.code == ToolErrorCode::InvalidParams);
    }
}

TEST_CASE("TimeTools:time.advance 无时间系统 PreconditionFailed") {
    ToolRegistry reg;
    reg.Register(MakeTimeAdvanceTool());
    me::scene::Scene scene;
    me::command::CommandStack stack;
    ToolInvocationLog log;
    ToolContext ctx{scene, stack, log}; // time = nullptr

    auto r = reg.Invoke("time.advance", {{"minutes", 10}}, CallerRole::Automation, ctx);
    CHECK_FALSE(r.ok);
    CHECK(r.code == ToolErrorCode::PreconditionFailed);
}

TEST_CASE("TimeTools:time.advance 权限——Agent 被拒") {
    ToolRegistry reg;
    reg.Register(MakeTimeAdvanceTool());
    me::scene::Scene scene;
    me::command::CommandStack stack;
    ToolInvocationLog log;
    me::domain::TimeSystem ts(SmallConfig());
    ToolContext ctx{scene, stack, log, &ts};

    auto r = reg.Invoke("time.advance", {{"minutes", 10}}, CallerRole::Agent, ctx);
    CHECK_FALSE(r.ok);
    CHECK(r.code == ToolErrorCode::PermissionDenied); // Automation 权限,Agent 不足
}
```

- [ ] **Step 2: 构建 + 跑,确认新用例失败**

Run:
```bash
cmake --build build-wsl -j && ./build-wsl/bin/me_tests -tc="TimeTools:time.advance*"
```
Expected: 这些用例 FAIL(`MakeTimeAdvanceTool` 仍是桩 `return nullptr;` → Register 失败 → UnknownTool)。

- [ ] **Step 3: 实现 time.advance**

在 `engine/toolapi/src/tools/TimeTools.cpp` 顶部(`CalendarToJson` 之后)加 TimeStep 序列化辅助:

```cpp
nlohmann::json TimeStepToJson(const me::domain::TimeStep& s) {
    return nlohmann::json{
        {"minutesAdvanced", s.minutesAdvanced}, {"daysCrossed", s.daysCrossed},
        {"seasonsCrossed", s.seasonsCrossed},   {"yearsCrossed", s.yearsCrossed},
    };
}
```

在匿名命名空间内(`TimeGetTool` 之后)加类:

```cpp
// time.advance:推进 N 分钟。变更运行时态——刻意不经 CommandStack(时间不可 Undo,见 ADR 0006)。
// dry-run 在值拷贝副本上推进 → 零副作用。
class TimeAdvanceTool final : public ITool {
public:
    std::string name() const override { return "time.advance"; }
    ToolCategory category() const override { return ToolCategory::Mutation; }
    Permission permission() const override { return Permission::Automation; }
    nlohmann::json paramsSchema() const override {
        return {{"type", "object"},
                {"required", {"minutes"}},
                {"properties", {{"minutes", {{"type", "integer"}, {"minimum", 1}}}}}};
    }
    ToolResult dryRun(ToolContext& ctx, const nlohmann::json& p) const override {
        if (ctx.time == nullptr)
            return ToolResult::Error(ToolErrorCode::PreconditionFailed,
                                     "no time system wired into ToolContext");
        me::domain::TimeSystem preview = *ctx.time; // 值拷贝:在副本上推进即零副作用
        const me::domain::TimeStep step = preview.Advance(p["minutes"].get<int>());
        return ToolResult::Success(
            {{"step", TimeStepToJson(step)}, {"time", CalendarToJson(preview.Now())}});
    }
    ToolResult run(ToolContext& ctx, const nlohmann::json& p) const override {
        if (ctx.time == nullptr)
            return ToolResult::Error(ToolErrorCode::PreconditionFailed,
                                     "no time system wired into ToolContext");
        const me::domain::TimeStep step = ctx.time->Advance(p["minutes"].get<int>());
        return ToolResult::Success(
            {{"step", TimeStepToJson(step)}, {"time", CalendarToJson(ctx.time->Now())}});
    }
};
```

把文件末尾的桩 `std::unique_ptr<ITool> MakeTimeAdvanceTool() { return nullptr; }`(若 Task 4 未建,则新增)替换为:

```cpp
std::unique_ptr<ITool> MakeTimeAdvanceTool() { return std::make_unique<TimeAdvanceTool>(); }
```

> 若 Task 4 的 TimeTools.cpp 末尾没有 `MakeTimeAdvanceTool` 桩,本步骤直接新增上面这一行工厂定义即可。

- [ ] **Step 4: 注册 + 整合测试到 8**

`engine/toolapi/src/tools/BuiltinTools.cpp` —— 在 `registry.Register(MakeTimeGetTool());` 后加:

```cpp
    registry.Register(MakeTimeAdvanceTool());
```

`tests/toolapi/test_builtin_integration.cpp` —— 7 改 8:

```cpp
TEST_CASE("Integration:RegisterBuiltinTools 注册全部 8 个 Tool") {
    ToolRegistry reg;
    RegisterBuiltinTools(reg);
    CHECK(reg.Size() == 8);
    auto names = reg.ListNames();
    CHECK(names[0] == "entity.set_transform");
    CHECK(names[1] == "log.read");
}
```

- [ ] **Step 5: 构建 + 跑全量测试,确认通过**

Run:
```bash
cmake --build build-wsl -j && ctest --test-dir build-wsl --output-on-failure
```
Expected: `TimeTools:time.advance*` 全 PASS;整合测试 8 PASS;无回归。记下总用例数(应较 M7 的 142 增加)。

- [ ] **Step 6: 提交**

```bash
git add engine/toolapi tests/toolapi/test_time_tools.cpp tests/toolapi/test_builtin_integration.cpp
git commit -m "feat(toolapi): time.advance 推进 Tool(dry-run 零副作用,运行时态不进 Command)"
```

---

### Task 6: 数据驱动示例资产 + 文档回写(README / PROGRESS / ADR 0006)

交付示例 JSON 配置(体现"参数从外部文件"原则)并把里程碑成果落档。

**Files:**
- Create: `assets/config/time.json`
- Modify: `README.md`(模块列表加 `me_domain`)
- Modify: `docs/PROGRESS.md`(一句话现状 + 里程碑表 + 下一步 + ADR 摘要 + 文档索引)
- Create: `docs/architecture/0006-m8-1-time-system.md`

- [ ] **Step 1: 写示例配置** —— `assets/config/time.json`

```json
{
    "minutesPerDay": 1440,
    "daysPerSeason": 28,
    "seasonsPerYear": 4,
    "seasonNames": ["Spring", "Summer", "Fall", "Winter"],
    "gameMinutesPerStep": 10,
    "realSecondsPerStep": 7.0,
    "startYear": 1,
    "startSeason": 0,
    "startDay": 1,
    "startMinute": 360
}
```

- [ ] **Step 2: 校验示例配置可被 LoadTimeConfig 接受**(用一次性内联检查,避免裸断言遗漏)

把 `assets/config/time.json` 的内容粘进一个临时 doctest 用例或用已通过的 `TimeConfig:合法 JSON 全字段解析`(其 `ValidJson()` 取值与此一致)间接确认。无需新增测试文件——仅人工核对两处取值一致(minutesPerDay 1440 / 四季名 / startMinute 360)。

- [ ] **Step 3: 更新 README 模块列表**

`README.md` 的"## 模块"小节,在 `me_editor` 条目后加一行:

```markdown
- `engine/domain`(`me_domain`):农场领域层。M8.1 时间系统(四级日历 + TimeSystem,数据驱动,CPU-only)。
```

- [ ] **Step 4: 写 ADR 0006** —— `docs/architecture/0006-m8-1-time-system.md`

```markdown
# ADR 0006:M8.1 时间系统

- 日期:2026-06-21
- 状态:已接受

## 背景
M8 农场领域层拆为多切片;M8.1 先做时间系统——作物生长/NPC 日程的共同地基。

## 决策
1. **新增 CPU-only `me_domain` 模块**(`→ me_core + nlohmann_json`),承载后续作物/库存/NPC。纯逻辑可 WSL doctest。
2. **四级日历(分钟/天/季/年),全参数 JSON 驱动**;`LoadTimeConfig` 返回 `std::optional`,不抛异常。
3. **内部用单调 `int64 totalMinutes_` 单一真相源**,日历字段即时派生;跨界计数 = floor 除法之差。否决多字段进位(易错)与 double 秒(漂移)。
4. **推进返回 `TimeStep` 践跳计数**(非布尔/非回调),消费者轮询;契合项目"显式传参、零全局状态"。
5. **开 `time.get`(AgentAllowed)/ `time.advance`(Automation)两 Tool**,经既有 Registry 流水线;`ToolContext` 加可选 `TimeSystem*`(前置声明隔离,默认 nullptr,保持 M6/M7 构造有效)。
6. **time.advance 是 Mutation 但刻意不经 CommandStack**:时间是运行时状态、Undo 无意义。dry-run 通过 TimeSystem 值拷贝在副本上推进实现零副作用。此为"变更经 Command"约定的文档化例外(该约定针对场景编辑)。

## 后果
- 时间系统单独红绿,作物(M8.2 消费 daysCrossed)/NPC(M8.4 消费 minuteOfDay)有稳定地基。
- `totalMinutes_` 即时间的完整序列化表示,为未来存档铺路。
- 未做 `time.set`/暂停/倍速(YAGNI),待消费场景明确再加。
```

- [ ] **Step 5: 回写 PROGRESS.md**

- 顶部"当前阶段":改为 `M8.1 时间系统完成(WSL 全绿);下一步 M8.2 作物生长`。
- "一句话现状":在 M7 段后追加一句 M8.1 摘要(me_domain + 四级日历 + TimeStep + time.get/advance + 用例数)。
- 里程碑表:`M8 农场领域层` 行状态改 ◐,说明加"M8.1 时间系统 ☑"。
- "文档索引"表:加 M8.1 设计、M8.1 计划、ADR 0006 三行。
- "下一步行动":M8.1 标记完成,新增 M8.2 作物生长(消费 `TimeStep.daysCrossed`)为下一步。
- "关键决策记录(ADR 摘要)"表:追加 6 行对应 ADR 0006 的六条决策(日期 2026-06-21)。

> 具体文字以实现时仓库最新内容为准,保持与既有条目同样的简洁风格。

- [ ] **Step 6: 全量回归 + 提交**

Run:
```bash
cmake --build build-wsl -j && ctest --test-dir build-wsl --output-on-failure
```
Expected: 全绿(确认文档任务未引入构建/资产问题)。

```bash
git add assets/config/time.json README.md docs/PROGRESS.md docs/architecture/0006-m8-1-time-system.md
git commit -m "docs(m8.1): 示例时间配置资产 + README/PROGRESS/ADR 0006 回写"
```

---

## 验收标准回顾(对照 spec §10)

- [ ] `me_domain` 建库,根 + 测试 CMake 同步,WSL 全量 doctest 绿。
- [ ] 四级派生 + Advance 跨界 + Update 累加 + TimeStep 计数语义均有测试覆盖。
- [ ] `time.get` / `time.advance` 经 Registry 流水线(白名单 + schema + dry-run + 审计)验证。
- [ ] `engine/domain/README.md` 记录职责、计数基准、依赖方向。
- [ ] PROGRESS.md 回写 + ADR 0006。
- [ ] `assets/config/time.json` 体现数据驱动(取值与测试一致)。
