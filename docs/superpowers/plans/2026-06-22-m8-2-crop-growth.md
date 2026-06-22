# M8.2 作物生长 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 `me_domain` 实现浇水驱动的作物生长核心循环(种植→浇水→跨天推进→成熟→收获),经 5 个受控 Tool 暴露,全程 CPU-only doctest 红绿。

**Architecture:** 纯 CPU 领域逻辑落在 `me_domain`(沿用 `TimeSystem` 先例):`CropDatabase`(JSON 数据驱动作物表)+ `FarmField`(以瓦片坐标为键的运行时作物网格,值语义可拷贝供 dry-run)。ToolAPI 层新增 `CropTools.cpp`,经既有 `ToolRegistry` 流水线暴露 5 Tool;作物变更为运行时态,**不经 CommandStack**(沿用 ADR 0006 例外),dry-run 用 `FarmField` 值拷贝副本预演。

**Tech Stack:** C++17、nlohmann/json(配置 + Tool 边界)、doctest(单测)、CMake。

## Global Constraints

- C++17;无 C++ 异常 —— 可恢复错误用 `std::optional` / 返回值 / `ToolResult`,不变量违反用 `ME_ASSERT`。
- 零硬编码数值/内容:作物阶段数、产量等全部来自 `crops.json`,源码无裸数字。
- 不使用 Singleton / 全局可变状态;引擎访问经显式传参的 `ToolContext`。
- 头文件中禁止 `using namespace std`。
- 模块依赖严格单向:`me_domain → me_core + nlohmann/json`,**严禁** domain 依赖 scene/command/toolapi/rhi/renderer。`me_toolapi → command/scene/core/domain`。
- 新增模块/源文件必须同步更新对应 `CMakeLists.txt`。
- 全程 CPU-only,WSL 下 doctest 红绿,无 Windows/GPU 依赖。
- 工作分支:在 `main` 之外新建 `feature/M8.2` 分支(首个 commit 前)。
- 每个 commit message 以 `Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>` 结尾。

## File Structure

**新建:**
- `engine/domain/include/me/domain/CropConfig.h` — `CropConfig` / `CropDatabase` 结构 + `LoadCropDatabase`。
- `engine/domain/src/CropConfig.cpp` — 解析 + 校验实现。
- `engine/domain/include/me/domain/FarmField.h` — `TileKey` / `CropInstance` / 结果类型 / `FarmField` 类。
- `engine/domain/src/FarmField.cpp` — 状态机实现。
- `engine/toolapi/src/tools/CropTools.cpp` — 5 个 crop Tool + 序列化工厂。
- `tests/domain/test_crop_config.cpp` — 配置加载/校验测试。
- `tests/domain/test_farm_field.cpp` — 状态机测试。
- `tests/toolapi/test_crop_tools.cpp` — Tool 流水线测试。
- `assets/config/crops.json` — 示例作物表。
- `docs/architecture/0007-m8-2-crop-growth.md` — ADR。

**修改:**
- `engine/domain/CMakeLists.txt` — 加 `src/CropConfig.cpp`、`src/FarmField.cpp`。
- `engine/toolapi/include/me/toolapi/tools/BuiltinTools.h` — 加 5 工厂声明 + 序列化声明。
- `engine/toolapi/src/tools/BuiltinTools.cpp` — `RegisterBuiltinTools` 追加注册。
- `engine/toolapi/include/me/toolapi/ToolContext.h` — 加 `me::domain::FarmField* farm`。
- `engine/toolapi/CMakeLists.txt` — 加 `src/tools/CropTools.cpp`。
- `tests/CMakeLists.txt` — 加 3 个测试源文件。
- `engine/domain/README.md` — 追加 `CropDatabase`/`FarmField` 说明。
- `docs/PROGRESS.md` — 回写 M8.2 完成。

---

### Task 1: CropDatabase(数据驱动作物表)

**Files:**
- Create: `engine/domain/include/me/domain/CropConfig.h`
- Create: `engine/domain/src/CropConfig.cpp`
- Modify: `engine/domain/CMakeLists.txt`
- Test: `tests/domain/test_crop_config.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: nlohmann/json。
- Produces:
  - `struct me::domain::CropConfig { std::string id; std::string name; std::vector<std::string> stageNames; std::vector<int> stageDays; std::string harvestItemId; int yield; }`
  - `CropConfig` 方法:`int StageCount() const`、`int LastStage() const`(`StageCount()-1`)、`bool IsMatureStage(int stage) const`。
  - `class CropDatabase`,方法 `const CropConfig* Find(const std::string& id) const`(未命中 nullptr)、`std::size_t Size() const`。
  - `std::optional<CropDatabase> me::domain::LoadCropDatabase(const nlohmann::json& j)`(顶层须为数组;任一作物非法或 id 重复 → `nullopt`)。

- [ ] **Step 1: 新建分支**

Run:
```bash
git checkout -b feature/M8.2
```
Expected: `Switched to a new branch 'feature/M8.2'`

- [ ] **Step 2: 写头文件 `CropConfig.h`**

```cpp
#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace me::domain {

/**
 * @brief 单种作物的数据驱动配置(全部从 JSON 加载,源码零硬编码数值)。
 *
 * stageNames 与 stageDays 一一对应,0 基;最后一个阶段即成熟/可收获阶段。
 * stageDays[i] = 进入下一阶段所需的“已浇水天数”;最后阶段的值生长上不使用。
 */
struct CropConfig {
    std::string id;                       ///< 唯一非空标识
    std::string name;                     ///< 展示名
    std::vector<std::string> stageNames;  ///< 阶段名(0 基)
    std::vector<int> stageDays;           ///< 各阶段所需已浇水天数(每项 ≥1)
    std::string harvestItemId;            ///< 收获产出物品 id
    int yield = 0;                        ///< 单株产量(≥1)

    /// @brief 阶段总数。
    int StageCount() const { return static_cast<int>(stageNames.size()); }
    /// @brief 最后(成熟)阶段索引。
    int LastStage() const { return StageCount() - 1; }
    /// @brief 给定阶段是否为成熟阶段。
    bool IsMatureStage(int stage) const { return stage == LastStage(); }
};

/// @brief 作物表:id → CropConfig 的只读集合。
class CropDatabase {
public:
    CropDatabase() = default;

    /// @brief 查作物;未命中返回 nullptr。
    const CropConfig* Find(const std::string& id) const;
    /// @brief 作物种类数。
    std::size_t Size() const { return crops_.size(); }

private:
    friend std::optional<CropDatabase> LoadCropDatabase(const nlohmann::json&);
    std::vector<CropConfig> crops_;
};

/**
 * @brief 从 JSON 数组解析并校验作物表。
 * @return 校验通过返回数据库;顶层非数组/任一作物字段缺失/类型错/语义越界/id 重复
 *         返回 std::nullopt(不抛异常)。
 */
std::optional<CropDatabase> LoadCropDatabase(const nlohmann::json& j);

} // namespace me::domain
```

- [ ] **Step 3: 写实现 `CropConfig.cpp`**

```cpp
#include "me/domain/CropConfig.h"

#include <algorithm>

namespace me::domain {

const CropConfig* CropDatabase::Find(const std::string& id) const {
    for (const auto& c : crops_) {
        if (c.id == id) return &c;
    }
    return nullptr;
}

namespace {

// 解析并校验单条作物;任一不满足返回 false。
bool ReadCrop(const nlohmann::json& j, CropConfig& out) {
    if (!j.is_object()) return false;

    if (!j.contains("id") || !j["id"].is_string()) return false;
    out.id = j["id"].get<std::string>();
    if (out.id.empty()) return false;

    if (!j.contains("name") || !j["name"].is_string()) return false;
    out.name = j["name"].get<std::string>();
    if (out.name.empty()) return false;

    if (!j.contains("harvestItemId") || !j["harvestItemId"].is_string()) return false;
    out.harvestItemId = j["harvestItemId"].get<std::string>();
    if (out.harvestItemId.empty()) return false;

    if (!j.contains("yield") || !j["yield"].is_number_integer()) return false;
    out.yield = j["yield"].get<int>();
    if (out.yield < 1) return false;

    if (!j.contains("stageNames") || !j["stageNames"].is_array()) return false;
    for (const auto& n : j["stageNames"]) {
        if (!n.is_string()) return false;
        out.stageNames.push_back(n.get<std::string>());
    }

    if (!j.contains("stageDays") || !j["stageDays"].is_array()) return false;
    for (const auto& d : j["stageDays"]) {
        if (!d.is_number_integer()) return false;
        const int days = d.get<int>();
        if (days < 1) return false;
        out.stageDays.push_back(days);
    }

    // 语义不变量:阶段名与天数一一对应,且至少一个阶段。
    if (out.stageNames.empty()) return false;
    if (out.stageNames.size() != out.stageDays.size()) return false;

    return true;
}

} // namespace

std::optional<CropDatabase> LoadCropDatabase(const nlohmann::json& j) {
    if (!j.is_array()) return std::nullopt;

    CropDatabase db;
    for (const auto& item : j) {
        CropConfig c;
        if (!ReadCrop(item, c)) return std::nullopt;
        // id 唯一性
        for (const auto& existing : db.crops_) {
            if (existing.id == c.id) return std::nullopt;
        }
        db.crops_.push_back(std::move(c));
    }
    return db;
}

} // namespace me::domain
```

- [ ] **Step 4: 注册到 `engine/domain/CMakeLists.txt`**

把 `add_library(me_domain STATIC ...)` 改为:
```cmake
add_library(me_domain STATIC
    src/TimeConfig.cpp
    src/TimeSystem.cpp
    src/CropConfig.cpp
    src/FarmField.cpp
)
```
(`FarmField.cpp` 在 Task 2 创建;为避免来回改 CMake,本步一次性加入两源文件。先创建一个最小占位 `FarmField.cpp`:仅含 `#include "me/domain/FarmField.h"`,但该头此刻不存在 → 构建会失败。**因此本步先只加 `src/CropConfig.cpp`**;`src/FarmField.cpp` 留到 Task 2 Step 4 再加。)

最终本步写入:
```cmake
add_library(me_domain STATIC
    src/TimeConfig.cpp
    src/TimeSystem.cpp
    src/CropConfig.cpp
)
```

- [ ] **Step 5: 写失败测试 `tests/domain/test_crop_config.cpp`**

```cpp
#include <doctest/doctest.h>

#include "me/domain/CropConfig.h"

using me::domain::LoadCropDatabase;

namespace {
// 一份合法作物表(与 assets/config/crops.json 取值严格一致;仅测试内用)。
nlohmann::json ValidCropJson() {
    return nlohmann::json::array({
        {{"id", "parsnip"},
         {"name", "Parsnip"},
         {"stageNames", {"seed", "sprout", "growing", "mature"}},
         {"stageDays", {1, 1, 1, 1}},
         {"harvestItemId", "parsnip"},
         {"yield", 1}},
        {{"id", "cauliflower"},
         {"name", "Cauliflower"},
         {"stageNames", {"seed", "sprout", "leafy", "heading", "mature"}},
         {"stageDays", {2, 2, 2, 2, 2}},
         {"harvestItemId", "cauliflower"},
         {"yield", 1}},
    });
}
} // namespace

TEST_CASE("CropConfig:合法作物表全字段解析") {
    auto db = LoadCropDatabase(ValidCropJson());
    REQUIRE(db.has_value());
    CHECK(db->Size() == 2);

    const auto* p = db->Find("parsnip");
    REQUIRE(p != nullptr);
    CHECK(p->name == "Parsnip");
    CHECK(p->StageCount() == 4);
    CHECK(p->LastStage() == 3);
    CHECK(p->stageDays[0] == 1);
    CHECK(p->harvestItemId == "parsnip");
    CHECK(p->yield == 1);
    CHECK(p->IsMatureStage(3));
    CHECK_FALSE(p->IsMatureStage(2));

    CHECK(db->Find("cauliflower") != nullptr);
    CHECK(db->Find("nonexistent") == nullptr);
}

TEST_CASE("CropConfig:非法配置返回 nullopt") {
    SUBCASE("顶层非数组") {
        CHECK_FALSE(LoadCropDatabase(nlohmann::json::object()).has_value());
    }
    SUBCASE("stageNames 与 stageDays 长度不匹配") {
        auto j = ValidCropJson();
        j[0]["stageDays"] = {1, 1, 1}; // 4 名 vs 3 天
        CHECK_FALSE(LoadCropDatabase(j).has_value());
    }
    SUBCASE("stageDays 含 <1") {
        auto j = ValidCropJson();
        j[0]["stageDays"] = {1, 0, 1, 1};
        CHECK_FALSE(LoadCropDatabase(j).has_value());
    }
    SUBCASE("yield < 1") {
        auto j = ValidCropJson();
        j[0]["yield"] = 0;
        CHECK_FALSE(LoadCropDatabase(j).has_value());
    }
    SUBCASE("空 id") {
        auto j = ValidCropJson();
        j[0]["id"] = "";
        CHECK_FALSE(LoadCropDatabase(j).has_value());
    }
    SUBCASE("id 重复") {
        auto j = ValidCropJson();
        j[1]["id"] = "parsnip";
        CHECK_FALSE(LoadCropDatabase(j).has_value());
    }
    SUBCASE("缺 harvestItemId") {
        auto j = ValidCropJson();
        j[0].erase("harvestItemId");
        CHECK_FALSE(LoadCropDatabase(j).has_value());
    }
    SUBCASE("stageNames 空") {
        auto j = ValidCropJson();
        j[0]["stageNames"] = nlohmann::json::array();
        j[0]["stageDays"] = nlohmann::json::array();
        CHECK_FALSE(LoadCropDatabase(j).has_value());
    }
    SUBCASE("yield 类型错(字符串)") {
        auto j = ValidCropJson();
        j[0]["yield"] = "1";
        CHECK_FALSE(LoadCropDatabase(j).has_value());
    }
}
```

- [ ] **Step 6: 把测试加入 `tests/CMakeLists.txt`**

在 `domain/test_time_system.cpp` 行之后加一行:
```cmake
    domain/test_crop_config.cpp
```

- [ ] **Step 7: 构建并运行,确认通过**

Run:
```bash
cmake --build build --target me_tests -j 2>&1 | tail -20 && ./build/tests/me_tests --test-case="CropConfig:*"
```
Expected: 编译通过;两个 TEST_CASE 全 PASS。
(若 `build` 目录不存在,先 `cmake -S . -B build` 配置。)

- [ ] **Step 8: 提交**

```bash
git add engine/domain/include/me/domain/CropConfig.h engine/domain/src/CropConfig.cpp engine/domain/CMakeLists.txt tests/domain/test_crop_config.cpp tests/CMakeLists.txt
git commit -m "feat(domain): CropDatabase 数据驱动作物表加载 + 校验

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 2: FarmField 数据模型 + Plant/Water/At

**Files:**
- Create: `engine/domain/include/me/domain/FarmField.h`
- Create: `engine/domain/src/FarmField.cpp`
- Modify: `engine/domain/CMakeLists.txt`
- Test: `tests/domain/test_farm_field.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `me::domain::CropDatabase`、`CropConfig`(Task 1)。
- Produces:
  - `struct TileKey { int x; int y; }` + `operator<`(字典序,供 `std::map`)。
  - `struct CropInstance { std::string cropId; int stage; int daysInStage; bool watered; }`。
  - `enum class PlantStatus { Ok, TileOccupied, UnknownCrop }`。
  - `class FarmField`:`explicit FarmField(CropDatabase db)`;`PlantStatus Plant(int x,int y,const std::string& cropId)`;`bool Water(int x,int y)`;`const CropInstance* At(int x,int y) const`;`const std::map<TileKey,CropInstance>& Crops() const`;`const CropDatabase& Database() const`。
  - (`AdvanceDays` / `Harvest` 在 Task 3/4 加入同一类。)

- [ ] **Step 1: 写头文件 `FarmField.h`**

```cpp
#pragma once

#include <map>
#include <string>

#include "me/domain/CropConfig.h"

namespace me::domain {

/// @brief 农田瓦片坐标键(整数格点)。字典序排序使遍历确定性。
struct TileKey {
    int x = 0;
    int y = 0;
};

inline bool operator<(const TileKey& a, const TileKey& b) {
    if (a.x != b.x) return a.x < b.x;
    return a.y < b.y;
}

/// @brief 一株作物的运行时状态。
struct CropInstance {
    std::string cropId;   ///< 指向 CropDatabase 的作物 id
    int stage = 0;        ///< 0 基,当前阶段索引
    int daysInStage = 0;  ///< 本阶段已累计的“已浇水天数”
    bool watered = false; ///< 今日是否已浇水
};

/// @brief Plant 结果。
enum class PlantStatus {
    Ok,            ///< 种植成功
    TileOccupied,  ///< 该瓦片已有作物
    UnknownCrop,   ///< cropId 不在数据库
};

/**
 * @brief 农田:以瓦片坐标为键的作物实例网格 + 浇水驱动生长状态机。
 *
 * 纯 CPU 运行时态,值语义可拷贝(Tool dry-run 在副本上预演 → 零副作用)。
 * 不进 Command/Undo(运行时态,见 ADR 0006/0007)。
 */
class FarmField {
public:
    /// @brief 注入作物表(值持有)。
    explicit FarmField(CropDatabase db);

    /// @brief 在空瓦片种植已知作物。
    PlantStatus Plant(int x, int y, const std::string& cropId);

    /// @brief 给瓦片上的作物浇水(幂等)。无作物返回 false。
    bool Water(int x, int y);

    /// @brief 查瓦片作物;空返回 nullptr。
    const CropInstance* At(int x, int y) const;

    /// @brief 只读遍历全部作物(键有序)。
    const std::map<TileKey, CropInstance>& Crops() const { return crops_; }

    /// @brief 只读访问作物表。
    const CropDatabase& Database() const { return db_; }

private:
    CropDatabase db_;
    std::map<TileKey, CropInstance> crops_;
};

} // namespace me::domain
```

- [ ] **Step 2: 写实现 `FarmField.cpp`(仅 Task 2 范围)**

```cpp
#include "me/domain/FarmField.h"

namespace me::domain {

FarmField::FarmField(CropDatabase db) : db_(std::move(db)) {}

PlantStatus FarmField::Plant(int x, int y, const std::string& cropId) {
    const TileKey key{x, y};
    if (crops_.find(key) != crops_.end()) return PlantStatus::TileOccupied;
    if (db_.Find(cropId) == nullptr) return PlantStatus::UnknownCrop;
    crops_[key] = CropInstance{cropId, /*stage=*/0, /*daysInStage=*/0, /*watered=*/false};
    return PlantStatus::Ok;
}

bool FarmField::Water(int x, int y) {
    auto it = crops_.find(TileKey{x, y});
    if (it == crops_.end()) return false;
    it->second.watered = true; // 幂等
    return true;
}

const CropInstance* FarmField::At(int x, int y) const {
    auto it = crops_.find(TileKey{x, y});
    return it == crops_.end() ? nullptr : &it->second;
}

} // namespace me::domain
```

- [ ] **Step 3: 把 `FarmField.cpp` 加入 `engine/domain/CMakeLists.txt`**

`add_library(me_domain STATIC ...)` 内 `src/CropConfig.cpp` 后加一行:
```cmake
    src/FarmField.cpp
```

- [ ] **Step 4: 写失败测试 `tests/domain/test_farm_field.cpp`**

```cpp
#include <doctest/doctest.h>

#include "me/domain/FarmField.h"

using namespace me::domain;

namespace {
// 两株作物:parsnip 4 阶段各 1 天;cauliflower 用于跨阶段测试。
CropDatabase MakeDb() {
    auto db = LoadCropDatabase(nlohmann::json::array({
        {{"id", "parsnip"},
         {"name", "Parsnip"},
         {"stageNames", {"seed", "sprout", "growing", "mature"}},
         {"stageDays", {1, 1, 1, 1}},
         {"harvestItemId", "parsnip"},
         {"yield", 1}},
        {{"id", "cauliflower"},
         {"name", "Cauliflower"},
         {"stageNames", {"seed", "sprout", "leafy", "heading", "mature"}},
         {"stageDays", {2, 2, 2, 2, 2}},
         {"harvestItemId", "cauliflower"},
         {"yield", 3}},
    }));
    REQUIRE(db.has_value());
    return *db;
}
} // namespace

TEST_CASE("FarmField:种植成功放入种子阶段") {
    FarmField field(MakeDb());
    CHECK(field.Plant(2, 3, "parsnip") == PlantStatus::Ok);
    const auto* c = field.At(2, 3);
    REQUIRE(c != nullptr);
    CHECK(c->cropId == "parsnip");
    CHECK(c->stage == 0);
    CHECK(c->daysInStage == 0);
    CHECK_FALSE(c->watered);
}

TEST_CASE("FarmField:重复种植同瓦片失败") {
    FarmField field(MakeDb());
    REQUIRE(field.Plant(2, 3, "parsnip") == PlantStatus::Ok);
    CHECK(field.Plant(2, 3, "cauliflower") == PlantStatus::TileOccupied);
}

TEST_CASE("FarmField:种植未知作物失败") {
    FarmField field(MakeDb());
    CHECK(field.Plant(0, 0, "banana") == PlantStatus::UnknownCrop);
    CHECK(field.At(0, 0) == nullptr);
}

TEST_CASE("FarmField:浇水标记") {
    FarmField field(MakeDb());
    REQUIRE(field.Plant(1, 1, "parsnip") == PlantStatus::Ok);
    CHECK(field.Water(1, 1));
    CHECK(field.At(1, 1)->watered);
    CHECK(field.Water(1, 1)); // 幂等
    CHECK(field.At(1, 1)->watered);
}

TEST_CASE("FarmField:浇空瓦片失败") {
    FarmField field(MakeDb());
    CHECK_FALSE(field.Water(5, 5));
}

TEST_CASE("FarmField:At 空瓦片返回 nullptr") {
    FarmField field(MakeDb());
    CHECK(field.At(9, 9) == nullptr);
}
```

- [ ] **Step 5: 把测试加入 `tests/CMakeLists.txt`**

在 `domain/test_crop_config.cpp` 后加:
```cmake
    domain/test_farm_field.cpp
```

- [ ] **Step 6: 构建并运行,确认通过**

Run:
```bash
cmake --build build --target me_tests -j 2>&1 | tail -20 && ./build/tests/me_tests --test-case="FarmField:*"
```
Expected: 编译通过;6 个 TEST_CASE 全 PASS。

- [ ] **Step 7: 提交**

```bash
git add engine/domain/include/me/domain/FarmField.h engine/domain/src/FarmField.cpp engine/domain/CMakeLists.txt tests/domain/test_farm_field.cpp tests/CMakeLists.txt
git commit -m "feat(domain): FarmField 数据模型 + Plant/Water/At

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 3: FarmField.AdvanceDays(浇水驱动状态机)

**Files:**
- Modify: `engine/domain/include/me/domain/FarmField.h`
- Modify: `engine/domain/src/FarmField.cpp`
- Test: `tests/domain/test_farm_field.cpp`(追加)

**Interfaces:**
- Consumes: Task 2 的 `FarmField`、`CropInstance`、`CropConfig`。
- Produces: `void FarmField::AdvanceDays(int n)`(`n ≥ 1`;对每株重复 n 次单日推进:已浇水则 `daysInStage++` 并清 `watered`,满阶段天数且非成熟阶段则进阶并归零;未浇水停滞;成熟阶段不再前进)。

- [ ] **Step 1: 在 `FarmField.h` 类 public 区加声明**

在 `bool Water(...)` 之后加:
```cpp
    /// @brief 推进 n 天(n≥1);仅“已浇水”的当天计入生长,未浇水停滞。
    ///        每天对每株:已浇水→daysInStage++、清浇水标记,满阶段天数且未成熟则进阶。
    void AdvanceDays(int n);
```

在类 private 区(字段之上)加内部辅助声明:
```cpp
    /// @brief 对所有作物推进单独一天。
    void AdvanceOneDay();
```

- [ ] **Step 2: 在 `FarmField.cpp` 加实现**

在 `Water` 实现之后、`At` 之前插入:
```cpp
void FarmField::AdvanceOneDay() {
    for (auto& [key, crop] : crops_) {
        (void)key;
        if (!crop.watered) continue; // 未浇水:停滞
        crop.watered = false;
        crop.daysInStage++;
        const CropConfig* cfg = db_.Find(crop.cropId);
        if (cfg == nullptr) continue; // 防御:配置缺失则不推进(理论不该发生)
        if (cfg->IsMatureStage(crop.stage)) continue; // 已成熟:不再前进
        if (crop.daysInStage >= cfg->stageDays[crop.stage]) {
            crop.stage++;
            crop.daysInStage = 0;
        }
    }
}

void FarmField::AdvanceDays(int n) {
    for (int i = 0; i < n; ++i) AdvanceOneDay();
}
```

- [ ] **Step 3: 在 `tests/domain/test_farm_field.cpp` 末尾追加测试**

```cpp
TEST_CASE("FarmField:浇水后推进一天进入下一阶段") {
    FarmField field(MakeDb());
    REQUIRE(field.Plant(0, 0, "parsnip") == PlantStatus::Ok); // 各阶段 1 天
    REQUIRE(field.Water(0, 0));
    field.AdvanceDays(1);
    const auto* c = field.At(0, 0);
    REQUIRE(c != nullptr);
    CHECK(c->stage == 1);
    CHECK(c->daysInStage == 0);
    CHECK_FALSE(c->watered); // 推进后浇水标记清除
}

TEST_CASE("FarmField:未浇水推进停滞") {
    FarmField field(MakeDb());
    REQUIRE(field.Plant(0, 0, "parsnip") == PlantStatus::Ok);
    field.AdvanceDays(3); // 从未浇水
    const auto* c = field.At(0, 0);
    CHECK(c->stage == 0);
    CHECK(c->daysInStage == 0);
}

TEST_CASE("FarmField:多阶段作物逐阶段推进(每阶段 2 天)") {
    FarmField field(MakeDb());
    REQUIRE(field.Plant(0, 0, "cauliflower") == PlantStatus::Ok); // 5 阶段各 2 天
    // 第 1 天浇水推进:daysInStage 1,仍 stage 0
    REQUIRE(field.Water(0, 0));
    field.AdvanceDays(1);
    CHECK(field.At(0, 0)->stage == 0);
    CHECK(field.At(0, 0)->daysInStage == 1);
    // 第 2 天浇水推进:满 2 天 → stage 1
    REQUIRE(field.Water(0, 0));
    field.AdvanceDays(1);
    CHECK(field.At(0, 0)->stage == 1);
    CHECK(field.At(0, 0)->daysInStage == 0);
}

TEST_CASE("FarmField:浇水→推进循环可达成熟阶段") {
    FarmField field(MakeDb());
    REQUIRE(field.Plant(0, 0, "parsnip") == PlantStatus::Ok); // 4 阶段各 1 天
    for (int day = 0; day < 3; ++day) { // 3 次进阶到 stage 3(成熟)
        REQUIRE(field.Water(0, 0));
        field.AdvanceDays(1);
    }
    const auto* c = field.At(0, 0);
    CHECK(c->stage == 3);
    CHECK(c->cropId == "parsnip");
}

TEST_CASE("FarmField:成熟后继续浇水推进不再前进") {
    FarmField field(MakeDb());
    REQUIRE(field.Plant(0, 0, "parsnip") == PlantStatus::Ok);
    for (int day = 0; day < 3; ++day) {
        REQUIRE(field.Water(0, 0));
        field.AdvanceDays(1);
    }
    REQUIRE(field.At(0, 0)->stage == 3); // 成熟
    field.Water(0, 0);
    field.AdvanceDays(5);
    CHECK(field.At(0, 0)->stage == 3); // 仍停在成熟
}
```

- [ ] **Step 4: 构建并运行,确认通过**

Run:
```bash
cmake --build build --target me_tests -j 2>&1 | tail -20 && ./build/tests/me_tests --test-case="FarmField:*"
```
Expected: 全部 FarmField TEST_CASE(含新增 5 个)PASS。

- [ ] **Step 5: 提交**

```bash
git add engine/domain/include/me/domain/FarmField.h engine/domain/src/FarmField.cpp tests/domain/test_farm_field.cpp
git commit -m "feat(domain): FarmField.AdvanceDays 浇水驱动生长状态机

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 4: FarmField.Harvest(收获产出 + 清空瓦片)

**Files:**
- Modify: `engine/domain/include/me/domain/FarmField.h`
- Modify: `engine/domain/src/FarmField.cpp`
- Test: `tests/domain/test_farm_field.cpp`(追加)

**Interfaces:**
- Consumes: Task 2/3 的 `FarmField`。
- Produces:
  - `enum class HarvestStatus { Ok, EmptyTile, NotMature }`。
  - `struct HarvestResult { HarvestStatus status; std::string itemId; int count; }`。
  - `HarvestResult FarmField::Harvest(int x, int y)`(成熟→产出 `{itemId,yield}` 并清空瓦片;空瓦片→`EmptyTile`;未成熟→`NotMature`)。

- [ ] **Step 1: 在 `FarmField.h` 加结果类型(`CropInstance` 之后、`PlantStatus` 附近)**

```cpp
/// @brief Harvest 结果状态。
enum class HarvestStatus {
    Ok,        ///< 收获成功
    EmptyTile, ///< 该瓦片无作物
    NotMature, ///< 作物未到成熟阶段
};

/// @brief Harvest 产出(status==Ok 时 itemId/count 有效)。
struct HarvestResult {
    HarvestStatus status = HarvestStatus::EmptyTile;
    std::string itemId;
    int count = 0;
};
```

并在类 public 区 `At` 之前加声明:
```cpp
    /// @brief 收获成熟作物:返回产出并清空该瓦片;未成熟/空瓦片返回对应状态。
    HarvestResult Harvest(int x, int y);
```

- [ ] **Step 2: 在 `FarmField.cpp` 加实现(`AdvanceDays` 之后)**

```cpp
HarvestResult FarmField::Harvest(int x, int y) {
    auto it = crops_.find(TileKey{x, y});
    if (it == crops_.end()) return HarvestResult{HarvestStatus::EmptyTile, {}, 0};

    const CropConfig* cfg = db_.Find(it->second.cropId);
    // cfg 理论恒存在(种植时已校验);防御性判空。
    if (cfg == nullptr || !cfg->IsMatureStage(it->second.stage))
        return HarvestResult{HarvestStatus::NotMature, {}, 0};

    HarvestResult out{HarvestStatus::Ok, cfg->harvestItemId, cfg->yield};
    crops_.erase(it); // 清空瓦片
    return out;
}
```

- [ ] **Step 3: 在 `tests/domain/test_farm_field.cpp` 末尾追加测试**

```cpp
namespace {
// 把瓦片上的作物推到成熟阶段(parsnip:3 次浇水+推进)。
void GrowParsnipToMature(FarmField& field, int x, int y) {
    REQUIRE(field.Plant(x, y, "parsnip") == PlantStatus::Ok);
    for (int day = 0; day < 3; ++day) {
        REQUIRE(field.Water(x, y));
        field.AdvanceDays(1);
    }
    REQUIRE(field.At(x, y)->stage == 3);
}
} // namespace

TEST_CASE("FarmField:成熟作物收获产出并清空瓦片") {
    FarmField field(MakeDb());
    GrowParsnipToMature(field, 4, 4);
    auto r = field.Harvest(4, 4);
    CHECK(r.status == HarvestStatus::Ok);
    CHECK(r.itemId == "parsnip");
    CHECK(r.count == 1);
    CHECK(field.At(4, 4) == nullptr); // 瓦片已清空
}

TEST_CASE("FarmField:产量取自配置(cauliflower yield=3)") {
    FarmField field(MakeDb());
    REQUIRE(field.Plant(0, 0, "cauliflower") == PlantStatus::Ok); // 5 阶段各 2 天
    for (int i = 0; i < 8; ++i) { // 4 次进阶 ×2 天 = 8 浇水日到 stage 4(成熟)
        REQUIRE(field.Water(0, 0));
        field.AdvanceDays(1);
    }
    REQUIRE(field.At(0, 0)->stage == 4);
    auto r = field.Harvest(0, 0);
    CHECK(r.status == HarvestStatus::Ok);
    CHECK(r.count == 3);
}

TEST_CASE("FarmField:未成熟收获失败且不清空") {
    FarmField field(MakeDb());
    REQUIRE(field.Plant(4, 4, "parsnip") == PlantStatus::Ok);
    auto r = field.Harvest(4, 4);
    CHECK(r.status == HarvestStatus::NotMature);
    CHECK(field.At(4, 4) != nullptr); // 仍在
}

TEST_CASE("FarmField:空瓦片收获返回 EmptyTile") {
    FarmField field(MakeDb());
    auto r = field.Harvest(7, 7);
    CHECK(r.status == HarvestStatus::EmptyTile);
}
```

- [ ] **Step 4: 构建并运行,确认通过**

Run:
```bash
cmake --build build --target me_tests -j 2>&1 | tail -20 && ./build/tests/me_tests --test-case="FarmField:*"
```
Expected: 全部 FarmField TEST_CASE(含新增 4 个)PASS。

- [ ] **Step 5: 提交**

```bash
git add engine/domain/include/me/domain/FarmField.h engine/domain/src/FarmField.cpp tests/domain/test_farm_field.cpp
git commit -m "feat(domain): FarmField.Harvest 收获产出 + 清空瓦片

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 5: ToolContext 接线 + 查询/种植/浇水 Tool

**Files:**
- Modify: `engine/toolapi/include/me/toolapi/ToolContext.h`
- Create: `engine/toolapi/src/tools/CropTools.cpp`
- Modify: `engine/toolapi/include/me/toolapi/tools/BuiltinTools.h`
- Modify: `engine/toolapi/src/tools/BuiltinTools.cpp`
- Modify: `engine/toolapi/CMakeLists.txt`
- Test: `tests/toolapi/test_crop_tools.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `me::domain::FarmField`(Task 2-4)、`ITool`、`ToolContext`、`ToolResult`、`ToolRegistry`、`Permission`/`CallerRole`。
- Produces:
  - `ToolContext` 新字段 `me::domain::FarmField* farm = nullptr`。
  - `nlohmann::json me::toolapi::CropInstanceToJson(int x, int y, const me::domain::CropInstance& c, const me::domain::CropDatabase& db)`(返回 `{x,y,cropId,stage,stageName,daysInStage,watered,mature}`)。
  - 工厂:`std::unique_ptr<ITool> MakeCropGetFieldTool()`、`MakeCropPlantTool()`、`MakeCropWaterTool()`。
  - `crop.get_field`(Query/AgentAllowed)、`crop.plant`(Mutation/Automation)、`crop.water`(Mutation/Automation)注册进 `RegisterBuiltinTools`。

- [ ] **Step 1: 扩展 `ToolContext.h`**

在 `namespace me::domain { class TimeSystem; }` 行后加前置声明:
```cpp
namespace me::domain { class FarmField; }   // 前置声明:仅按指针使用
```
在 struct 内 `me::domain::TimeSystem* time = nullptr;` 之后加:
```cpp
    me::domain::FarmField* farm = nullptr;   ///< 可选:crop Tool 数据源,缺省 nullptr
```

- [ ] **Step 2: 在 `BuiltinTools.h` 加声明**

在 `namespace me::domain { struct CalendarTime; }` 行旁加:
```cpp
namespace me::domain { struct CropInstance; class CropDatabase; }
```
在时间型工厂声明之后加:
```cpp
/// @brief CropInstance → JSON { x,y,cropId,stage,stageName,daysInStage,watered,mature }。
nlohmann::json CropInstanceToJson(int x, int y, const me::domain::CropInstance& c,
                                  const me::domain::CropDatabase& db);

// —— 作物型 Tool 工厂(M8.2)——
std::unique_ptr<ITool> MakeCropGetFieldTool(); ///< crop.get_field
std::unique_ptr<ITool> MakeCropPlantTool();    ///< crop.plant
std::unique_ptr<ITool> MakeCropWaterTool();    ///< crop.water
std::unique_ptr<ITool> MakeCropAdvanceDaysTool(); ///< crop.advance_days(Task 6)
std::unique_ptr<ITool> MakeCropHarvestTool();  ///< crop.harvest(Task 6)
```

- [ ] **Step 3: 写 `CropTools.cpp`(Task 5 范围:序列化 + 3 Tool)**

```cpp
#include <memory>

#include "me/domain/FarmField.h"
#include "me/toolapi/ToolContext.h"
#include "me/toolapi/tools/BuiltinTools.h"

namespace me::toolapi {

nlohmann::json CropInstanceToJson(int x, int y, const me::domain::CropInstance& c,
                                  const me::domain::CropDatabase& db) {
    const me::domain::CropConfig* cfg = db.Find(c.cropId);
    std::string stageName = cfg ? cfg->stageNames[c.stage] : std::string{};
    bool mature = cfg ? cfg->IsMatureStage(c.stage) : false;
    return nlohmann::json{
        {"x", x},           {"y", y},
        {"cropId", c.cropId},{"stage", c.stage},
        {"stageName", stageName},{"daysInStage", c.daysInStage},
        {"watered", c.watered},{"mature", mature},
    };
}

namespace {

// 把整片农田序列化为 { crops:[...] }(键有序,确定性)。
nlohmann::json FieldToJson(const me::domain::FarmField& farm) {
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& [key, crop] : farm.Crops())
        arr.push_back(CropInstanceToJson(key.x, key.y, crop, farm.Database()));
    return nlohmann::json{{"crops", arr}};
}

// crop.get_field:返回全部作物(只读)。
class CropGetFieldTool final : public ITool {
public:
    std::string name() const override { return "crop.get_field"; }
    ToolCategory category() const override { return ToolCategory::Query; }
    Permission permission() const override { return Permission::AgentAllowed; }
    nlohmann::json paramsSchema() const override {
        return {{"type", "object"}, {"properties", nlohmann::json::object()}};
    }
    ToolResult dryRun(ToolContext& ctx, const nlohmann::json& p) const override {
        return run(ctx, p); // 只读:预览即结果
    }
    ToolResult run(ToolContext& ctx, const nlohmann::json&) const override {
        if (ctx.farm == nullptr)
            return ToolResult::Error(ToolErrorCode::PreconditionFailed,
                                     "no farm field wired into ToolContext");
        return ToolResult::Success(FieldToJson(*ctx.farm));
    }
};

// crop.plant:在瓦片种植作物。运行时态,不经 CommandStack。
class CropPlantTool final : public ITool {
public:
    std::string name() const override { return "crop.plant"; }
    ToolCategory category() const override { return ToolCategory::Mutation; }
    Permission permission() const override { return Permission::Automation; }
    nlohmann::json paramsSchema() const override {
        return {{"type", "object"},
                {"required", {"tileX", "tileY", "cropId"}},
                {"properties",
                 {{"tileX", {{"type", "integer"}}},
                  {"tileY", {{"type", "integer"}}},
                  {"cropId", {{"type", "string"}}}}}};
    }
    ToolResult dryRun(ToolContext& ctx, const nlohmann::json& p) const override {
        if (ctx.farm == nullptr)
            return ToolResult::Error(ToolErrorCode::PreconditionFailed,
                                     "no farm field wired into ToolContext");
        me::domain::FarmField preview = *ctx.farm; // 值拷贝:零副作用
        return apply(preview, p);
    }
    ToolResult run(ToolContext& ctx, const nlohmann::json& p) const override {
        if (ctx.farm == nullptr)
            return ToolResult::Error(ToolErrorCode::PreconditionFailed,
                                     "no farm field wired into ToolContext");
        return apply(*ctx.farm, p);
    }
private:
    static ToolResult apply(me::domain::FarmField& farm, const nlohmann::json& p) {
        const int x = p["tileX"].get<int>();
        const int y = p["tileY"].get<int>();
        const std::string cropId = p["cropId"].get<std::string>();
        switch (farm.Plant(x, y, cropId)) {
            case me::domain::PlantStatus::TileOccupied:
                return ToolResult::Error(ToolErrorCode::PreconditionFailed,
                                         "tile already has a crop");
            case me::domain::PlantStatus::UnknownCrop:
                return ToolResult::Error(ToolErrorCode::PreconditionFailed,
                                         "unknown cropId");
            case me::domain::PlantStatus::Ok:
                break;
        }
        return ToolResult::Success(
            CropInstanceToJson(x, y, *farm.At(x, y), farm.Database()));
    }
};

// crop.water:给瓦片浇水。运行时态,不经 CommandStack。
class CropWaterTool final : public ITool {
public:
    std::string name() const override { return "crop.water"; }
    ToolCategory category() const override { return ToolCategory::Mutation; }
    Permission permission() const override { return Permission::Automation; }
    nlohmann::json paramsSchema() const override {
        return {{"type", "object"},
                {"required", {"tileX", "tileY"}},
                {"properties",
                 {{"tileX", {{"type", "integer"}}},
                  {"tileY", {{"type", "integer"}}}}}};
    }
    ToolResult dryRun(ToolContext& ctx, const nlohmann::json& p) const override {
        if (ctx.farm == nullptr)
            return ToolResult::Error(ToolErrorCode::PreconditionFailed,
                                     "no farm field wired into ToolContext");
        me::domain::FarmField preview = *ctx.farm;
        return apply(preview, p);
    }
    ToolResult run(ToolContext& ctx, const nlohmann::json& p) const override {
        if (ctx.farm == nullptr)
            return ToolResult::Error(ToolErrorCode::PreconditionFailed,
                                     "no farm field wired into ToolContext");
        return apply(*ctx.farm, p);
    }
private:
    static ToolResult apply(me::domain::FarmField& farm, const nlohmann::json& p) {
        const int x = p["tileX"].get<int>();
        const int y = p["tileY"].get<int>();
        if (!farm.Water(x, y))
            return ToolResult::Error(ToolErrorCode::PreconditionFailed,
                                     "no crop on tile to water");
        return ToolResult::Success(
            CropInstanceToJson(x, y, *farm.At(x, y), farm.Database()));
    }
};

} // namespace

std::unique_ptr<ITool> MakeCropGetFieldTool() { return std::make_unique<CropGetFieldTool>(); }
std::unique_ptr<ITool> MakeCropPlantTool() { return std::make_unique<CropPlantTool>(); }
std::unique_ptr<ITool> MakeCropWaterTool() { return std::make_unique<CropWaterTool>(); }

} // namespace me::toolapi
```

> 注:`MakeCropAdvanceDaysTool` / `MakeCropHarvestTool` 在 Task 6 同文件内补齐;本 Task 不定义它们,故 Step 5 的 `RegisterBuiltinTools` 暂不注册这两个(否则链接缺符号)。

- [ ] **Step 4: 把 `CropTools.cpp` 加入 `engine/toolapi/CMakeLists.txt`**

`add_library(me_toolapi STATIC ...)` 内 `src/tools/TimeTools.cpp` 后加:
```cmake
    src/tools/CropTools.cpp
```

- [ ] **Step 5: 在 `BuiltinTools.cpp` 注册 3 个 Tool**

`RegisterBuiltinTools` 内时间型注册之后加:
```cpp
    // 作物型(M8.2)
    registry.Register(MakeCropGetFieldTool());
    registry.Register(MakeCropPlantTool());
    registry.Register(MakeCropWaterTool());
```

- [ ] **Step 6: 写测试 `tests/toolapi/test_crop_tools.cpp`(Task 5 范围)**

```cpp
#include <doctest/doctest.h>

#include "me/command/CommandStack.h"
#include "me/domain/FarmField.h"
#include "me/scene/Scene.h"
#include "me/toolapi/ToolContext.h"
#include "me/toolapi/ToolRegistry.h"
#include "me/toolapi/tools/BuiltinTools.h"

using namespace me::toolapi;

namespace {
me::domain::FarmField MakeFarm() {
    auto db = me::domain::LoadCropDatabase(nlohmann::json::array({
        {{"id", "parsnip"},
         {"name", "Parsnip"},
         {"stageNames", {"seed", "sprout", "growing", "mature"}},
         {"stageDays", {1, 1, 1, 1}},
         {"harvestItemId", "parsnip"},
         {"yield", 1}},
    }));
    REQUIRE(db.has_value());
    return me::domain::FarmField(*db);
}
} // namespace

TEST_CASE("CropTools:crop.plant 种植并 get_field 可见") {
    ToolRegistry reg;
    reg.Register(MakeCropPlantTool());
    reg.Register(MakeCropGetFieldTool());
    me::scene::Scene scene;
    me::command::CommandStack stack;
    ToolInvocationLog log;
    auto farm = MakeFarm();
    ToolContext ctx{scene, stack, log, nullptr, &farm};

    auto r = reg.Invoke("crop.plant", {{"tileX", 2}, {"tileY", 3}, {"cropId", "parsnip"}},
                        CallerRole::Automation, ctx);
    REQUIRE(r.ok);
    CHECK(r.data["cropId"] == "parsnip");
    CHECK(r.data["stage"] == 0);
    CHECK(r.data["mature"] == false);

    auto g = reg.Invoke("crop.get_field", nlohmann::json::object(), CallerRole::Agent, ctx);
    REQUIRE(g.ok);
    REQUIRE(g.data["crops"].size() == 1);
    CHECK(g.data["crops"][0]["x"] == 2);
    CHECK(g.data["crops"][0]["y"] == 3);
}

TEST_CASE("CropTools:crop.plant 重复瓦片 PreconditionFailed") {
    ToolRegistry reg;
    reg.Register(MakeCropPlantTool());
    me::scene::Scene scene;
    me::command::CommandStack stack;
    ToolInvocationLog log;
    auto farm = MakeFarm();
    ToolContext ctx{scene, stack, log, nullptr, &farm};

    REQUIRE(reg.Invoke("crop.plant", {{"tileX", 0}, {"tileY", 0}, {"cropId", "parsnip"}},
                       CallerRole::Automation, ctx).ok);
    auto r = reg.Invoke("crop.plant", {{"tileX", 0}, {"tileY", 0}, {"cropId", "parsnip"}},
                        CallerRole::Automation, ctx);
    CHECK_FALSE(r.ok);
    CHECK(r.code == ToolErrorCode::PreconditionFailed);
}

TEST_CASE("CropTools:crop.plant 未知作物 PreconditionFailed") {
    ToolRegistry reg;
    reg.Register(MakeCropPlantTool());
    me::scene::Scene scene;
    me::command::CommandStack stack;
    ToolInvocationLog log;
    auto farm = MakeFarm();
    ToolContext ctx{scene, stack, log, nullptr, &farm};

    auto r = reg.Invoke("crop.plant", {{"tileX", 0}, {"tileY", 0}, {"cropId", "banana"}},
                        CallerRole::Automation, ctx);
    CHECK_FALSE(r.ok);
    CHECK(r.code == ToolErrorCode::PreconditionFailed);
}

TEST_CASE("CropTools:crop.plant dry-run 零副作用") {
    ToolRegistry reg;
    reg.Register(MakeCropPlantTool());
    reg.Register(MakeCropGetFieldTool());
    me::scene::Scene scene;
    me::command::CommandStack stack;
    ToolInvocationLog log;
    auto farm = MakeFarm();
    ToolContext ctx{scene, stack, log, nullptr, &farm};

    auto r = reg.Invoke("crop.plant", {{"tileX", 1}, {"tileY", 1}, {"cropId", "parsnip"}},
                        CallerRole::Automation, ctx, /*dryRun=*/true);
    REQUIRE(r.ok);
    CHECK(farm.At(1, 1) == nullptr); // 真身未变
}

TEST_CASE("CropTools:crop.water 浇水标记") {
    ToolRegistry reg;
    reg.Register(MakeCropPlantTool());
    reg.Register(MakeCropWaterTool());
    me::scene::Scene scene;
    me::command::CommandStack stack;
    ToolInvocationLog log;
    auto farm = MakeFarm();
    ToolContext ctx{scene, stack, log, nullptr, &farm};

    REQUIRE(reg.Invoke("crop.plant", {{"tileX", 0}, {"tileY", 0}, {"cropId", "parsnip"}},
                       CallerRole::Automation, ctx).ok);
    auto r = reg.Invoke("crop.water", {{"tileX", 0}, {"tileY", 0}}, CallerRole::Automation, ctx);
    REQUIRE(r.ok);
    CHECK(r.data["watered"] == true);
}

TEST_CASE("CropTools:crop.water 空瓦片 PreconditionFailed") {
    ToolRegistry reg;
    reg.Register(MakeCropWaterTool());
    me::scene::Scene scene;
    me::command::CommandStack stack;
    ToolInvocationLog log;
    auto farm = MakeFarm();
    ToolContext ctx{scene, stack, log, nullptr, &farm};

    auto r = reg.Invoke("crop.water", {{"tileX", 9}, {"tileY", 9}}, CallerRole::Automation, ctx);
    CHECK_FALSE(r.ok);
    CHECK(r.code == ToolErrorCode::PreconditionFailed);
}

TEST_CASE("CropTools:crop.get_field 无农田 PreconditionFailed") {
    ToolRegistry reg;
    reg.Register(MakeCropGetFieldTool());
    me::scene::Scene scene;
    me::command::CommandStack stack;
    ToolInvocationLog log;
    ToolContext ctx{scene, stack, log}; // farm = nullptr

    auto r = reg.Invoke("crop.get_field", nlohmann::json::object(), CallerRole::Agent, ctx);
    CHECK_FALSE(r.ok);
    CHECK(r.code == ToolErrorCode::PreconditionFailed);
}

TEST_CASE("CropTools:crop.plant schema 拒绝缺字段") {
    ToolRegistry reg;
    reg.Register(MakeCropPlantTool());
    me::scene::Scene scene;
    me::command::CommandStack stack;
    ToolInvocationLog log;
    auto farm = MakeFarm();
    ToolContext ctx{scene, stack, log, nullptr, &farm};

    auto r = reg.Invoke("crop.plant", {{"tileX", 0}, {"tileY", 0}},
                        CallerRole::Automation, ctx);
    CHECK_FALSE(r.ok);
    CHECK(r.code == ToolErrorCode::InvalidParams);
}
```

> `ToolContext{scene, stack, log, nullptr, &farm}` 的第 4 实参是 `time`(nullptr),第 5 是 `farm`。聚合初始化顺序须与 `ToolContext.h` 字段顺序一致。

- [ ] **Step 7: 把测试加入 `tests/CMakeLists.txt`**

在 `toolapi/test_time_tools.cpp` 后加:
```cmake
    toolapi/test_crop_tools.cpp
```

- [ ] **Step 8: 构建并运行,确认通过**

Run:
```bash
cmake --build build --target me_tests -j 2>&1 | tail -20 && ./build/tests/me_tests --test-case="CropTools:*"
```
Expected: 编译通过;8 个 CropTools TEST_CASE 全 PASS。

- [ ] **Step 9: 提交**

```bash
git add engine/toolapi/ tests/toolapi/test_crop_tools.cpp tests/CMakeLists.txt
git commit -m "feat(toolapi): ToolContext 接 FarmField + crop.get_field/plant/water Tool

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 6: crop.advance_days + crop.harvest Tool

**Files:**
- Modify: `engine/toolapi/src/tools/CropTools.cpp`
- Modify: `engine/toolapi/src/tools/BuiltinTools.cpp`
- Test: `tests/toolapi/test_crop_tools.cpp`(追加)

**Interfaces:**
- Consumes: Task 5 的 `CropTools.cpp`(序列化 `CropInstanceToJson` / `FieldToJson`)、`FarmField.AdvanceDays`/`Harvest`(Task 3/4)。
- Produces: `MakeCropAdvanceDaysTool()`(crop.advance_days,Mutation/Automation,`{days:int,minimum:1}`,返回 `{advanced, crops:[...]}`)、`MakeCropHarvestTool()`(crop.harvest,Mutation/**EditorOnly**,`{tileX,tileY}`,返回 `{itemId,count}`)注册进 `RegisterBuiltinTools`。

- [ ] **Step 1: 在 `CropTools.cpp` 匿名命名空间内(`CropWaterTool` 之后)加两个 Tool**

```cpp
// crop.advance_days:推进 N 天(运行时态,不经 CommandStack)。
class CropAdvanceDaysTool final : public ITool {
public:
    std::string name() const override { return "crop.advance_days"; }
    ToolCategory category() const override { return ToolCategory::Mutation; }
    Permission permission() const override { return Permission::Automation; }
    nlohmann::json paramsSchema() const override {
        return {{"type", "object"},
                {"required", {"days"}},
                {"properties", {{"days", {{"type", "integer"}, {"minimum", 1}}}}}};
    }
    ToolResult dryRun(ToolContext& ctx, const nlohmann::json& p) const override {
        if (ctx.farm == nullptr)
            return ToolResult::Error(ToolErrorCode::PreconditionFailed,
                                     "no farm field wired into ToolContext");
        me::domain::FarmField preview = *ctx.farm; // 值拷贝:零副作用
        return apply(preview, p);
    }
    ToolResult run(ToolContext& ctx, const nlohmann::json& p) const override {
        if (ctx.farm == nullptr)
            return ToolResult::Error(ToolErrorCode::PreconditionFailed,
                                     "no farm field wired into ToolContext");
        return apply(*ctx.farm, p);
    }
private:
    static ToolResult apply(me::domain::FarmField& farm, const nlohmann::json& p) {
        const int days = p["days"].get<int>();
        farm.AdvanceDays(days);
        nlohmann::json out = FieldToJson(farm);
        out["advanced"] = days;
        return ToolResult::Success(out);
    }
};

// crop.harvest:收获成熟作物(EditorOnly:销毁性产出,清空瓦片)。运行时态,不经 CommandStack。
class CropHarvestTool final : public ITool {
public:
    std::string name() const override { return "crop.harvest"; }
    ToolCategory category() const override { return ToolCategory::Mutation; }
    Permission permission() const override { return Permission::EditorOnly; }
    nlohmann::json paramsSchema() const override {
        return {{"type", "object"},
                {"required", {"tileX", "tileY"}},
                {"properties",
                 {{"tileX", {{"type", "integer"}}},
                  {"tileY", {{"type", "integer"}}}}}};
    }
    ToolResult dryRun(ToolContext& ctx, const nlohmann::json& p) const override {
        if (ctx.farm == nullptr)
            return ToolResult::Error(ToolErrorCode::PreconditionFailed,
                                     "no farm field wired into ToolContext");
        me::domain::FarmField preview = *ctx.farm;
        return apply(preview, p);
    }
    ToolResult run(ToolContext& ctx, const nlohmann::json& p) const override {
        if (ctx.farm == nullptr)
            return ToolResult::Error(ToolErrorCode::PreconditionFailed,
                                     "no farm field wired into ToolContext");
        return apply(*ctx.farm, p);
    }
private:
    static ToolResult apply(me::domain::FarmField& farm, const nlohmann::json& p) {
        const int x = p["tileX"].get<int>();
        const int y = p["tileY"].get<int>();
        const me::domain::HarvestResult h = farm.Harvest(x, y);
        switch (h.status) {
            case me::domain::HarvestStatus::EmptyTile:
                return ToolResult::Error(ToolErrorCode::PreconditionFailed,
                                         "no crop on tile to harvest");
            case me::domain::HarvestStatus::NotMature:
                return ToolResult::Error(ToolErrorCode::PreconditionFailed,
                                         "crop not mature");
            case me::domain::HarvestStatus::Ok:
                break;
        }
        return ToolResult::Success({{"itemId", h.itemId}, {"count", h.count}});
    }
};
```

- [ ] **Step 2: 在 `CropTools.cpp` 文件底部加两个工厂(匿名命名空间外,`me::toolapi` 内)**

在已有 `MakeCropWaterTool` 工厂之后加:
```cpp
std::unique_ptr<ITool> MakeCropAdvanceDaysTool() {
    return std::make_unique<CropAdvanceDaysTool>();
}
std::unique_ptr<ITool> MakeCropHarvestTool() {
    return std::make_unique<CropHarvestTool>();
}
```

- [ ] **Step 3: 在 `BuiltinTools.cpp` 注册两个 Tool**

作物型注册块内 `MakeCropWaterTool()` 之后加:
```cpp
    registry.Register(MakeCropAdvanceDaysTool());
    registry.Register(MakeCropHarvestTool());
```

- [ ] **Step 4: 在 `tests/toolapi/test_crop_tools.cpp` 末尾追加测试**

```cpp
TEST_CASE("CropTools:crop.advance_days 推进并回报 advanced") {
    ToolRegistry reg;
    reg.Register(MakeCropPlantTool());
    reg.Register(MakeCropWaterTool());
    reg.Register(MakeCropAdvanceDaysTool());
    me::scene::Scene scene;
    me::command::CommandStack stack;
    ToolInvocationLog log;
    auto farm = MakeFarm();
    ToolContext ctx{scene, stack, log, nullptr, &farm};

    REQUIRE(reg.Invoke("crop.plant", {{"tileX", 0}, {"tileY", 0}, {"cropId", "parsnip"}},
                       CallerRole::Automation, ctx).ok);
    REQUIRE(reg.Invoke("crop.water", {{"tileX", 0}, {"tileY", 0}},
                       CallerRole::Automation, ctx).ok);
    auto r = reg.Invoke("crop.advance_days", {{"days", 1}}, CallerRole::Automation, ctx);
    REQUIRE(r.ok);
    CHECK(r.data["advanced"] == 1);
    CHECK(r.data["crops"][0]["stage"] == 1);
    CHECK(farm.At(0, 0)->stage == 1); // 真实状态推进
}

TEST_CASE("CropTools:crop.advance_days dry-run 零副作用") {
    ToolRegistry reg;
    reg.Register(MakeCropPlantTool());
    reg.Register(MakeCropWaterTool());
    reg.Register(MakeCropAdvanceDaysTool());
    me::scene::Scene scene;
    me::command::CommandStack stack;
    ToolInvocationLog log;
    auto farm = MakeFarm();
    ToolContext ctx{scene, stack, log, nullptr, &farm};

    REQUIRE(reg.Invoke("crop.plant", {{"tileX", 0}, {"tileY", 0}, {"cropId", "parsnip"}},
                       CallerRole::Automation, ctx).ok);
    REQUIRE(reg.Invoke("crop.water", {{"tileX", 0}, {"tileY", 0}},
                       CallerRole::Automation, ctx).ok);
    auto r = reg.Invoke("crop.advance_days", {{"days", 1}}, CallerRole::Automation, ctx,
                        /*dryRun=*/true);
    REQUIRE(r.ok);
    CHECK(r.data["crops"][0]["stage"] == 1); // 预览前进
    CHECK(farm.At(0, 0)->stage == 0);         // 真身未变
}

TEST_CASE("CropTools:crop.advance_days schema 拒绝 days<1") {
    ToolRegistry reg;
    reg.Register(MakeCropAdvanceDaysTool());
    me::scene::Scene scene;
    me::command::CommandStack stack;
    ToolInvocationLog log;
    auto farm = MakeFarm();
    ToolContext ctx{scene, stack, log, nullptr, &farm};

    auto r = reg.Invoke("crop.advance_days", {{"days", 0}}, CallerRole::Automation, ctx);
    CHECK_FALSE(r.ok);
    CHECK(r.code == ToolErrorCode::InvalidParams);
}

TEST_CASE("CropTools:crop.harvest 成熟收获产出") {
    ToolRegistry reg;
    reg.Register(MakeCropPlantTool());
    reg.Register(MakeCropWaterTool());
    reg.Register(MakeCropAdvanceDaysTool());
    reg.Register(MakeCropHarvestTool());
    me::scene::Scene scene;
    me::command::CommandStack stack;
    ToolInvocationLog log;
    auto farm = MakeFarm();
    ToolContext ctx{scene, stack, log, nullptr, &farm};

    REQUIRE(reg.Invoke("crop.plant", {{"tileX", 0}, {"tileY", 0}, {"cropId", "parsnip"}},
                       CallerRole::Automation, ctx).ok);
    for (int day = 0; day < 3; ++day) { // 推到成熟(stage 3)
        REQUIRE(reg.Invoke("crop.water", {{"tileX", 0}, {"tileY", 0}},
                           CallerRole::Automation, ctx).ok);
        REQUIRE(reg.Invoke("crop.advance_days", {{"days", 1}},
                           CallerRole::Automation, ctx).ok);
    }
    auto r = reg.Invoke("crop.harvest", {{"tileX", 0}, {"tileY", 0}}, CallerRole::Editor, ctx);
    REQUIRE(r.ok);
    CHECK(r.data["itemId"] == "parsnip");
    CHECK(r.data["count"] == 1);
    CHECK(farm.At(0, 0) == nullptr); // 瓦片已清空
}

TEST_CASE("CropTools:crop.harvest 未成熟 PreconditionFailed") {
    ToolRegistry reg;
    reg.Register(MakeCropPlantTool());
    reg.Register(MakeCropHarvestTool());
    me::scene::Scene scene;
    me::command::CommandStack stack;
    ToolInvocationLog log;
    auto farm = MakeFarm();
    ToolContext ctx{scene, stack, log, nullptr, &farm};

    REQUIRE(reg.Invoke("crop.plant", {{"tileX", 0}, {"tileY", 0}, {"cropId", "parsnip"}},
                       CallerRole::Automation, ctx).ok);
    auto r = reg.Invoke("crop.harvest", {{"tileX", 0}, {"tileY", 0}}, CallerRole::Editor, ctx);
    CHECK_FALSE(r.ok);
    CHECK(r.code == ToolErrorCode::PreconditionFailed);
}

TEST_CASE("CropTools:crop.harvest 权限——Agent/Automation 被拒") {
    ToolRegistry reg;
    reg.Register(MakeCropHarvestTool());
    me::scene::Scene scene;
    me::command::CommandStack stack;
    ToolInvocationLog log;
    auto farm = MakeFarm();
    ToolContext ctx{scene, stack, log, nullptr, &farm};

    auto a = reg.Invoke("crop.harvest", {{"tileX", 0}, {"tileY", 0}}, CallerRole::Agent, ctx);
    CHECK(a.code == ToolErrorCode::PermissionDenied);
    auto m = reg.Invoke("crop.harvest", {{"tileX", 0}, {"tileY", 0}}, CallerRole::Automation, ctx);
    CHECK(m.code == ToolErrorCode::PermissionDenied); // EditorOnly
}
```

- [ ] **Step 5: 构建并运行,确认通过**

Run:
```bash
cmake --build build --target me_tests -j 2>&1 | tail -20 && ./build/tests/me_tests --test-case="CropTools:*"
```
Expected: 编译通过;全部 CropTools TEST_CASE(含新增 6 个)PASS。

- [ ] **Step 6: 全量回归,确认无破坏**

Run:
```bash
./build/tests/me_tests 2>&1 | tail -5
```
Expected: 全部测试 PASS(原 162 + 本里程碑新增,无 failure)。

- [ ] **Step 7: 提交**

```bash
git add engine/toolapi/src/tools/CropTools.cpp engine/toolapi/src/tools/BuiltinTools.cpp tests/toolapi/test_crop_tools.cpp
git commit -m "feat(toolapi): crop.advance_days + crop.harvest Tool(harvest EditorOnly)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 7: 资产 + 文档(ADR / README / PROGRESS)

**Files:**
- Create: `assets/config/crops.json`
- Create: `docs/architecture/0007-m8-2-crop-growth.md`
- Modify: `engine/domain/README.md`
- Modify: `docs/PROGRESS.md`

**Interfaces:** 无代码接口;文档 + 数据资产。

- [ ] **Step 1: 写 `assets/config/crops.json`(取值须与 `test_crop_config.cpp` 的 `ValidCropJson()` 严格一致)**

```json
[
  {
    "id": "parsnip",
    "name": "Parsnip",
    "stageNames": ["seed", "sprout", "growing", "mature"],
    "stageDays": [1, 1, 1, 1],
    "harvestItemId": "parsnip",
    "yield": 1
  },
  {
    "id": "cauliflower",
    "name": "Cauliflower",
    "stageNames": ["seed", "sprout", "leafy", "heading", "mature"],
    "stageDays": [2, 2, 2, 2, 2],
    "harvestItemId": "cauliflower",
    "yield": 1
  }
]
```

- [ ] **Step 2: 验证资产可被加载器解析(临时一次性检查)**

Run:
```bash
python3 -c "import json; json.load(open('assets/config/crops.json')); print('valid json')"
```
Expected: `valid json`(确保格式无误;真正语义校验由 `LoadCropDatabase` 单测的 `ValidCropJson()` 镜像覆盖)。

- [ ] **Step 3: 写 ADR `docs/architecture/0007-m8-2-crop-growth.md`**

```markdown
# ADR 0007:M8.2 作物生长

- 日期:2026-06-22
- 状态:已采纳
- 相关:[ADR 0006 时间系统](0006-m8-1-time-system.md)、[M8.2 设计](../superpowers/specs/2026-06-22-m8-2-crop-growth-design.md)

## 背景

M8.1 落地时间系统(`TimeSystem`/`TimeStep`)。M8.2 在其上实现农场核心玩法第二环:作物生长。

## 决策

1. **作物存 `me_domain` 独立 `FarmField`**(以瓦片坐标为键的 `std::map` 网格),非 Scene 组件。沿用 `TimeSystem` 先例:纯 CPU 可 doctest,与 Scene/RHI 解耦;渲染上屏留后续切片。
2. **浇水驱动状态机**:`CropInstance.daysInStage` 计“已浇水天数”而非自然天。`AdvanceDays` 每天仅推进已浇水的作物并清浇水标记,未浇水停滞(不死亡),成熟阶段不再前进。
3. **5 个 Tool**(`crop.get_field`/`plant`/`water`/`advance_days`/`harvest`),Builtin 总数 8→13。作物变更为**运行时游戏态,不经 CommandStack**——沿用 ADR 0006 文档化例外(“变更经 Command”是场景编辑约定,不适用运行时态);dry-run 用 `FarmField` 值拷贝副本预演,零副作用。
4. **`crop.harvest` = EditorOnly** 权限梯度:销毁性产出(清空瓦片),与 `scene.destroy_entity` 同档,演示三层白名单;查询 AgentAllowed、plant/water/advance Automation。
5. **收获产出不入库**:`crop.harvest` 把 `{itemId,count}` 作为 `ToolResult` 值返回,不写库存(库存是 M8.3,YAGNI)。
6. **时间与农场解耦**:`crop.advance_days(days)` 显式接受天数;调用方读 `time.advance` 的 `TimeStep.daysCrossed` 再喂入,`time.advance` 不内联农场。
7. **作物表数据驱动**:`assets/config/crops.json` 经 `LoadCropDatabase` 加载校验,取值与单测 `ValidCropJson()` 严格一致;源码零硬编码数值。

## 后果

- 作物生长逻辑全 CPU,WSL doctest 红绿,无 Windows/GPU 依赖。
- 产出未入库、作物未上屏、无季节锁定/枯萎/重生——均为后续里程碑/切片(YAGNI)。
- `FarmField` 值语义可拷贝是 dry-run 零副作用的基石(同 `TimeSystem`)。
```

- [ ] **Step 4: 更新 `engine/domain/README.md`**

在时间系统说明之后追加一节(措辞与既有文档风格一致):
```markdown
## 作物生长(M8.2)

- `CropConfig` / `CropDatabase`(`CropConfig.h`):数据驱动作物表,`LoadCropDatabase(json)` 返回 `std::optional`(顶层数组、字段校验、id 唯一)。
- `FarmField`(`FarmField.h`):以 `TileKey` 为键的作物实例网格 + 浇水驱动生长状态机。`Plant`/`Water`/`AdvanceDays`/`Harvest`/`At`/`Crops`/`Database`;值语义可拷贝供 Tool dry-run。
- 运行时态,不进 Command/Undo(见 ADR 0007)。经 toolapi 的 `crop.*` 5 Tool 暴露。
```

- [ ] **Step 5: 回写 `docs/PROGRESS.md`**

做四处更新:
1. 顶部「最后更新」「当前阶段」改为 M8.2 完成、下一步 M8.3 库存/物品。
2. 「里程碑进度」表 M8 行:把「作物生长(M8.2)」标 ☑,说明补一句(FarmField + CropDatabase + 5 Tool + doctest 全绿)。
3. 「下一步行动」第 3 条 M8.2 标 ☑,新增第 4 条 M8.3 库存/物品。
4. 「关键决策记录」表追加两行:
   - `| 2026-06-22 | 作物存 me_domain 独立 FarmField(非 Scene 组件)+ 浇水驱动状态机(daysInStage 计已浇水天数) | 沿用 TimeSystem 先例纯 CPU 可测;未浇水停滞符合星露谷核心循环(见 ADR 0007) |`
   - `| 2026-06-22 | 5 crop Tool;crop 变更不经 CommandStack(运行时态例外),harvest=EditorOnly | 沿用 ADR 0006 例外;harvest 销毁性产出收紧权限;产出不入库留 M8.3 |`
5. 「一句话现状」末尾追加 M8.2 段落(参照 M8.1 段落风格,概述 FarmField/CropDatabase/5 Tool/测试数)。

(回写完成后用 `./build/tests/me_tests 2>&1 | tail -3` 取实际通过数填入「一句话现状」。)

- [ ] **Step 6: 提交**

```bash
git add assets/config/crops.json docs/architecture/0007-m8-2-crop-growth.md engine/domain/README.md docs/PROGRESS.md
git commit -m "docs(M8.2): crops.json 资产 + ADR 0007 + README/PROGRESS 回写

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Self-Review

**1. Spec coverage:**
- 架构/模块归属(spec §2)→ Task 1/2(me_domain 两文件对)。✓
- 数据模型 CropConfig/CropInstance(spec §3)→ Task 1/2。✓
- 浇水驱动状态机(spec §4)→ Task 3。✓
- Harvest 产出+清空(spec §4.1)→ Task 4。✓
- ToolContext.farm + 5 Tool + 权限梯度 + dry-run(spec §5)→ Task 5/6。✓
- 错误处理(spec §6,`InvalidParams`/`PreconditionFailed`)→ Task 1(nullopt)、Task 5/6(Tool 翻译)。✓
- 测试三层(spec §7)→ Task 1(config)、2-4(FarmField)、5-6(Tool)。✓
- 资产 + 文档(spec §8/§9)→ Task 7。✓

**2. Placeholder scan:** 各步均含完整代码/命令/期望输出;无 TBD/TODO。✓

**3. Type consistency:**
- `PlantStatus{Ok,TileOccupied,UnknownCrop}`、`HarvestStatus{Ok,EmptyTile,NotMature}`、`HarvestResult{status,itemId,count}` 在 Task 2/4 定义,Task 5/6 Tool 一致引用。✓
- `FarmField` 方法名 `Plant/Water/AdvanceDays/Harvest/At/Crops/Database` 跨 Task 一致。✓
- `CropInstanceToJson(x,y,c,db)` 签名 Task 5 定义,Task 6 复用 `FieldToJson`(同文件匿名命名空间)一致。✓
- `ToolContext` 聚合初始化顺序 `{scene,commands,log,time,farm}` 与字段定义顺序一致(Task 5 Step 1 + 测试)。✓
- 错误码用实际枚举 `ToolErrorCode::InvalidParams`(非 spec 笔误的 InvalidArgument)、`PreconditionFailed`、`PermissionDenied`。✓
- `reg.Invoke(name, params, role, ctx, dryRun)` 签名与既有 `test_time_tools.cpp` 一致。✓

无遗留问题。
