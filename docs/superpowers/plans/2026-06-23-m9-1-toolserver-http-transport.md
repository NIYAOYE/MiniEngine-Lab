# Tool API HTTP 传输层(无头 Tool 服务器)Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把进程内的 `ToolRegistry::Invoke` 暴露为本地 HTTP+JSON 服务,让网页编辑器前端与未来 LLM agent 经同一受控边界驱动 MiniEngine 的 13 个 Tool。

**Architecture:** 新增 `me_toolserver` 静态库,剖分为 `ToolDispatcher`(纯 `std::string → std::string` 逻辑核心,持有 `ToolContext` + `ToolRegistry`,内置互斥锁串行化调用,可全量 doctest)与 `HttpToolServer`(cpp-httplib 薄壳,只做路由收发)。新增无头可执行 `toolserver_app`:加载 time/crops 配置、组装 `ToolContext`、跑 HTTP 服务,不链接 RHI/DX12。延续 M7「EditorController(可测)vs ImGui(封死边界)」的剖分。

**Tech Stack:** C++17、CMake(FetchContent)、nlohmann/json、cpp-httplib(单头)、doctest。

## Global Constraints

- C++17(`CMAKE_CXX_STANDARD 17`,`CMAKE_CXX_EXTENSIONS OFF`);CMake `>= 3.20`。
- 无硬编码:端口/绑定地址/资产路径用具名 `constexpr` 或命令行覆盖;游戏数值从 `assets/config/*.json` 加载。
- 不使用 C++ 异常:JSON 解析用非抛变体 `nlohmann::json::parse(body, nullptr, false)` + `.is_discarded()`;可恢复失败走 `ToolResult{ok:false,code}`;不变量违反用 `ME_ASSERT`。
- 业务错误一律 `ToolResult` 模型(`HandleInvoke` 永不抛、永不返回非 JSON);HTTP 4xx/5xx 只留给协议级问题。
- 模块依赖严格单向:`me_toolserver → me_toolapi + me_domain + me_scene + me_command + me_core + nlohmann/json + cpp-httplib`;不被任何引擎模块反向依赖。
- 公开 API 写 Doxygen 注释;MSVC `/utf-8`(源文件含中文注释,已由顶层 CMake 全局施加)。
- 现有 193 doctest 全程零回归。
- 仅绑 `127.0.0.1`;定位为本地开发工具,无鉴权/HTTPS。

---

## File Structure

- `engine/toolserver/CMakeLists.txt` — 新模块构建脚本。
- `engine/toolserver/include/me/toolserver/ToolDispatcher.h` — 纯逻辑调度核心接口。
- `engine/toolserver/src/ToolDispatcher.cpp` — `HandleInvoke`/`HandleListTools` 实现 + 角色/枚举字符串化助手。
- `engine/toolserver/include/me/toolserver/HttpToolServer.h` — cpp-httplib 薄壳接口 + 具名端口/host 常量。
- `engine/toolserver/src/HttpToolServer.cpp` — 三路由注册 + `listen()`。
- `engine/toolserver/README.md` — 模块说明。
- `apps/toolserver/CMakeLists.txt` — 无头可执行构建脚本。
- `apps/toolserver/main.cpp` — 装配引擎状态 + 加载资产 + 跑服务。
- `tests/toolserver/test_tool_dispatcher.cpp` — `ToolDispatcher` 全量 doctest。
- `third_party/CMakeLists.txt`(改)— 增 cpp-httplib FetchContent。
- `CMakeLists.txt`(改)— 增 `add_subdirectory(engine/toolserver)` 与 `add_subdirectory(apps/toolserver)`。
- `tests/CMakeLists.txt`(改)— 增测试源 + 链接 `me_toolserver`。
- `docs/architecture/0008-m9-1-toolserver-http-transport.md`(新)、`docs/PROGRESS.md`(改)、spec 状态(改)。

> **范围说明(执行者必读):** 本切片**不**做 tmj→Scene 实体的加载器(当前代码无此能力,sandbox 是用渲染组件内联拼的)。`toolserver_app` 启动加载 **mandatory** 的 `time.json` + `crops.json`(失败退出),场景以**空场景**启动(前端经 `scene.create_entity` 编辑)。spec §6「加载 farm_demo 场景实体」的意图(连上即有内容)由 time/crop 真实数据兑现;tmj→Scene 种子留后续切片。

---

### Task 1: me_toolserver 模块骨架 + ToolDispatcher.HandleInvoke 正常路径 + dryRun

**Files:**
- Create: `engine/toolserver/include/me/toolserver/ToolDispatcher.h`
- Create: `engine/toolserver/src/ToolDispatcher.cpp`
- Create: `engine/toolserver/CMakeLists.txt`
- Create: `tests/toolserver/test_tool_dispatcher.cpp`
- Modify: `CMakeLists.txt`(增 `add_subdirectory(engine/toolserver)`)
- Modify: `tests/CMakeLists.txt`(增测试源 + 链接 `me_toolserver`)

**Interfaces:**
- Consumes: `me::toolapi::ToolContext`(聚合 `{Scene&, CommandStack&, ToolInvocationLog&, TimeSystem*, FarmField*}`)、`me::toolapi::ToolRegistry`(`Invoke(name, params, role, ctx, dryRun)` → `ToolResult`;`ListNames()`;`Find(name)` → `const ITool*`)、`me::toolapi::RegisterBuiltinTools(registry)`(注册 13 Tool)、`ToolResult::toJson()` → `{ok,code,message,data,invocationId}`、`me::domain::TimeSystem(TimeConfig)`、`me::domain::FarmField(CropDatabase)`、`me::domain::LoadCropDatabase(json)`。
- Produces:
  - `me::toolserver::ToolDispatcher`,构造 `ToolDispatcher(me::toolapi::ToolContext& ctx, me::toolapi::ToolRegistry& registry)`;
  - `std::string HandleInvoke(const std::string& jsonBody)`;
  - `std::string HandleListTools()`(本任务先留声明 + 返回空数组 `"[]"`,Task 4 补实现)。

- [ ] **Step 1: 写头文件 `engine/toolserver/include/me/toolserver/ToolDispatcher.h`**

```cpp
#pragma once

#include <mutex>
#include <string>

namespace me::toolapi { struct ToolContext; class ToolRegistry; }

namespace me::toolserver {

/**
 * @brief 纯逻辑 Tool 调度核心:JSON 字符串入,JSON 字符串出,不含 socket。
 *
 * 线程安全:内部互斥锁串行化每次调用(共享 Scene/Farm 状态需原子)。
 * HandleInvoke 对任何非法输入都返回结构化 {ok:false,code} 的 JSON,绝不抛异常。
 */
class ToolDispatcher {
public:
    /// @brief 注入受控上下文与已注册 Builtin Tool 的 registry。
    ToolDispatcher(me::toolapi::ToolContext& ctx, me::toolapi::ToolRegistry& registry);

    /// @brief 处理一次 /invoke:解析 body → Invoke → 序列化 ToolResult 为 JSON 字符串。
    /// @param jsonBody 形如 {"name":..,"params":{..}?,"role":..?,"dryRun":..?}。
    std::string HandleInvoke(const std::string& jsonBody);

    /// @brief 处理 /tools:返回 [{name,category,permission,paramsSchema}] 的 JSON 字符串。
    std::string HandleListTools();

private:
    me::toolapi::ToolContext& ctx_;
    me::toolapi::ToolRegistry& registry_;
    std::mutex mutex_; ///< 串行化 Invoke/ListTools(共享 Scene/Farm 状态)
};

} // namespace me::toolserver
```

- [ ] **Step 2: 写实现 `engine/toolserver/src/ToolDispatcher.cpp`**

```cpp
#include "me/toolserver/ToolDispatcher.h"

#include <optional>

#include <nlohmann/json.hpp>

#include "me/toolapi/Permission.h"
#include "me/toolapi/ToolContext.h"
#include "me/toolapi/ToolRegistry.h"
#include "me/toolapi/ToolResult.h"

namespace me::toolserver {
namespace {

using me::toolapi::CallerRole;
using me::toolapi::ToolErrorCode;
using me::toolapi::ToolResult;

/// @brief 字符串 → CallerRole;非法返回 nullopt。
std::optional<CallerRole> ParseRole(const std::string& s) {
    if (s == "Agent") return CallerRole::Agent;
    if (s == "Automation") return CallerRole::Automation;
    if (s == "Editor") return CallerRole::Editor;
    return std::nullopt;
}

/// @brief 把失败 ToolResult 序列化为 JSON 字符串(统一出口)。
std::string ErrorJson(ToolErrorCode code, const std::string& msg) {
    return ToolResult::Error(code, msg).toJson().dump();
}

} // namespace

ToolDispatcher::ToolDispatcher(me::toolapi::ToolContext& ctx, me::toolapi::ToolRegistry& registry)
    : ctx_(ctx), registry_(registry) {}

std::string ToolDispatcher::HandleInvoke(const std::string& jsonBody) {
    std::lock_guard<std::mutex> lock(mutex_);

    const nlohmann::json body = nlohmann::json::parse(jsonBody, nullptr, /*allow_exceptions=*/false);
    if (body.is_discarded() || !body.is_object()) {
        return ErrorJson(ToolErrorCode::InvalidParams, "request body is not a valid JSON object");
    }
    if (!body.contains("name") || !body["name"].is_string()) {
        return ErrorJson(ToolErrorCode::InvalidParams, "missing or non-string 'name'");
    }
    const std::string name = body["name"].get<std::string>();

    nlohmann::json params = nlohmann::json::object();
    if (body.contains("params")) {
        if (!body["params"].is_object()) {
            return ErrorJson(ToolErrorCode::InvalidParams, "'params' must be an object");
        }
        params = body["params"];
    }

    CallerRole role = CallerRole::Editor; // 缺省本地编辑器全权
    if (body.contains("role")) {
        if (!body["role"].is_string()) {
            return ErrorJson(ToolErrorCode::InvalidParams, "'role' must be a string");
        }
        const auto parsed = ParseRole(body["role"].get<std::string>());
        if (!parsed) {
            return ErrorJson(ToolErrorCode::InvalidParams, "unknown role");
        }
        role = *parsed;
    }

    bool dryRun = false;
    if (body.contains("dryRun")) {
        if (!body["dryRun"].is_boolean()) {
            return ErrorJson(ToolErrorCode::InvalidParams, "'dryRun' must be a boolean");
        }
        dryRun = body["dryRun"].get<bool>();
    }

    const ToolResult result = registry_.Invoke(name, params, role, ctx_, dryRun);
    return result.toJson().dump();
}

std::string ToolDispatcher::HandleListTools() {
    std::lock_guard<std::mutex> lock(mutex_);
    return nlohmann::json::array().dump(); // Task 4 补实现
}

} // namespace me::toolserver
```

- [ ] **Step 3: 写构建脚本 `engine/toolserver/CMakeLists.txt`**

```cmake
add_library(me_toolserver STATIC
    src/ToolDispatcher.cpp
)
target_include_directories(me_toolserver PUBLIC include)
target_compile_features(me_toolserver PUBLIC cxx_std_17)
# 单向依赖:toolserver → toolapi → command → scene → core,以及 toolapi/domain。
# 它是应用层装配,不被任何引擎模块反向依赖。
target_link_libraries(me_toolserver PUBLIC
    me_toolapi me_domain me_scene me_command me_core nlohmann_json::nlohmann_json)
```

- [ ] **Step 4: 接入顶层构建 `CMakeLists.txt`**

在 `add_subdirectory(engine/domain)` 之后新增一行:

```cmake
add_subdirectory(engine/toolserver)
```

- [ ] **Step 5: 写失败测试 `tests/toolserver/test_tool_dispatcher.cpp`**

```cpp
#include <doctest/doctest.h>

#include <nlohmann/json.hpp>

#include "me/command/CommandStack.h"
#include "me/domain/CropConfig.h"
#include "me/domain/FarmField.h"
#include "me/domain/TimeConfig.h"
#include "me/domain/TimeSystem.h"
#include "me/scene/Scene.h"
#include "me/toolapi/ToolContext.h"
#include "me/toolapi/ToolRegistry.h"
#include "me/toolapi/tools/BuiltinTools.h"
#include "me/toolserver/ToolDispatcher.h"

using nlohmann::json;

namespace {

me::domain::TimeConfig MakeTimeConfig() {
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

me::domain::CropDatabase MakeCropDb() {
    auto db = me::domain::LoadCropDatabase(json::array({
        {{"id", "parsnip"},
         {"name", "Parsnip"},
         {"stageNames", {"seed", "sprout", "growing", "mature"}},
         {"stageDays", {1, 1, 1, 1}},
         {"harvestItemId", "parsnip"},
         {"yield", 1}},
    }));
    REQUIRE(db.has_value());
    return *db;
}

/// @brief 拥有全部引擎状态 + 13 Tool 的可复用 dispatcher 夹具。
struct Fixture {
    me::scene::Scene scene;
    me::command::CommandStack stack;
    me::toolapi::ToolInvocationLog log;
    me::domain::TimeSystem time;
    me::domain::FarmField farm;
    me::toolapi::ToolRegistry registry;
    me::toolapi::ToolContext ctx;
    me::toolserver::ToolDispatcher dispatcher;

    Fixture()
        : time(MakeTimeConfig()),
          farm(MakeCropDb()),
          ctx{scene, stack, log, &time, &farm},
          dispatcher(ctx, registry) {
        me::toolapi::RegisterBuiltinTools(registry);
    }
};

} // namespace

TEST_CASE("ToolDispatcher:合法 invoke 创建实体并可见") {
    Fixture f;

    const json out = json::parse(f.dispatcher.HandleInvoke(R"({"name":"scene.create_entity"})"));
    REQUIRE(out["ok"] == true);
    CHECK(out["code"] == "Ok");
    CHECK(out["invocationId"].get<std::uint64_t>() > 0);

    const json listed = json::parse(f.dispatcher.HandleInvoke(R"({"name":"scene.list_entities"})"));
    REQUIRE(listed["ok"] == true);
    CHECK(listed["data"]["entities"].size() == 1);
}

TEST_CASE("ToolDispatcher:dryRun 零副作用") {
    Fixture f;

    const json dry = json::parse(
        f.dispatcher.HandleInvoke(R"({"name":"scene.create_entity","dryRun":true})"));
    REQUIRE(dry["ok"] == true);

    const json listed = json::parse(f.dispatcher.HandleInvoke(R"({"name":"scene.list_entities"})"));
    REQUIRE(listed["ok"] == true);
    CHECK(listed["data"]["entities"].size() == 0);
}
```

> 注:`scene.list_entities` 的结果字段名为 `data.entities`(沿用 M6 既有约定);若 doctest 报字段不符,执行者先 `Find` 对应 Tool 实现确认字段名后改测试断言,**不**改 Tool。

- [ ] **Step 6: 接入测试构建 `tests/CMakeLists.txt`**

在 `add_executable(me_tests ...)` 源列表的 `domain/test_farm_field.cpp` 之后加一行:

```cmake
    toolserver/test_tool_dispatcher.cpp
```

并在 `target_link_libraries(me_tests PRIVATE ...)` 末尾追加 `me_toolserver`:

```cmake
target_link_libraries(me_tests PRIVATE doctest::doctest me_core me_platform me_rhi_cpu me_assets me_scene me_command me_toolapi me_editor me_domain me_toolserver)
```

- [ ] **Step 7: 配置并运行测试,确认通过**

Run:
```bash
cmake -S . -B build-wsl -DME_BUILD_TESTS=ON >/dev/null && cmake --build build-wsl --target me_tests -j 2>&1 | tail -5 && ./build-wsl/bin/me_tests -ts="ToolDispatcher*"
```
Expected: 编译通过;新增 2 个 TEST_CASE 全 PASS(`test cases: 2 | 2 passed`)。

- [ ] **Step 8: 跑全量回归**

Run: `./build-wsl/bin/me_tests 2>&1 | tail -3`
Expected: `test cases: 195 | 195 passed`(193 旧 + 2 新)。

- [ ] **Step 9: 提交**

```bash
git add engine/toolserver tests/toolserver/test_tool_dispatcher.cpp CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat(toolserver): me_toolserver 模块 + ToolDispatcher.HandleInvoke(正常+dryRun)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 2: HandleInvoke 错误路径(6 类,均结构化 + HTTP 200 语义)

**Files:**
- Modify: `tests/toolserver/test_tool_dispatcher.cpp`(追加 TEST_CASE)

**Interfaces:**
- Consumes: Task 1 的 `Fixture`、`ToolDispatcher::HandleInvoke`、`ToolErrorCode` 稳定字符串(`InvalidParams`/`UnknownTool`/`PermissionDenied`/`PreconditionFailed`)。
- Produces: 无新增接口(纯测试,验证 Task 1 实现已覆盖全部错误分支)。

- [ ] **Step 1: 追加失败/边界测试到 `tests/toolserver/test_tool_dispatcher.cpp` 末尾**

```cpp
TEST_CASE("ToolDispatcher:body 非法 JSON → InvalidParams") {
    Fixture f;
    const json out = json::parse(f.dispatcher.HandleInvoke("{not json"));
    CHECK(out["ok"] == false);
    CHECK(out["code"] == "InvalidParams");
}

TEST_CASE("ToolDispatcher:body 非对象 → InvalidParams") {
    Fixture f;
    const json out = json::parse(f.dispatcher.HandleInvoke("[1,2,3]"));
    CHECK(out["ok"] == false);
    CHECK(out["code"] == "InvalidParams");
}

TEST_CASE("ToolDispatcher:缺 name → InvalidParams") {
    Fixture f;
    const json out = json::parse(f.dispatcher.HandleInvoke(R"({"params":{}})"));
    CHECK(out["ok"] == false);
    CHECK(out["code"] == "InvalidParams");
}

TEST_CASE("ToolDispatcher:未知 Tool → UnknownTool") {
    Fixture f;
    const json out = json::parse(f.dispatcher.HandleInvoke(R"({"name":"scene.no_such_tool"})"));
    CHECK(out["ok"] == false);
    CHECK(out["code"] == "UnknownTool");
}

TEST_CASE("ToolDispatcher:非法 role 字符串 → InvalidParams") {
    Fixture f;
    const json out = json::parse(
        f.dispatcher.HandleInvoke(R"({"name":"scene.list_entities","role":"Wizard"})"));
    CHECK(out["ok"] == false);
    CHECK(out["code"] == "InvalidParams");
}

TEST_CASE("ToolDispatcher:Agent 调 destroy(EditorOnly)→ PermissionDenied") {
    Fixture f;
    const json out = json::parse(f.dispatcher.HandleInvoke(
        R"({"name":"scene.destroy_entity","params":{"entityId":1},"role":"Agent"})"));
    CHECK(out["ok"] == false);
    CHECK(out["code"] == "PermissionDenied");
}

TEST_CASE("ToolDispatcher:destroy 不存在实体 → PreconditionFailed") {
    Fixture f;
    const json out = json::parse(f.dispatcher.HandleInvoke(
        R"({"name":"scene.destroy_entity","params":{"entityId":999},"role":"Editor"})"));
    CHECK(out["ok"] == false);
    CHECK(out["code"] == "PreconditionFailed");
}
```

> 注:`scene.destroy_entity` 的参数名(此处用 `entityId`)须与 M6 Tool schema 一致;若 schema 用别名,执行者先读 `engine/toolapi/src/tools/MutationTools.cpp` 确认后改测试参数名,**不**改 Tool。`PermissionDenied` 在 Schema 校验之前裁决,故即便 `entityId` 不存在也先返回权限错误。

- [ ] **Step 2: 运行新测试,确认通过**

Run: `cmake --build build-wsl --target me_tests -j 2>&1 | tail -3 && ./build-wsl/bin/me_tests -ts="ToolDispatcher*" 2>&1 | tail -3`
Expected: 9 个 ToolDispatcher 用例全 PASS。

- [ ] **Step 3: 提交**

```bash
git add tests/toolserver/test_tool_dispatcher.cpp
git commit -m "test(toolserver): HandleInvoke 6 类错误路径(结构化 ToolResult)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 3: 角色解析裁决(同一 Tool 不同 role 不同结果)

**Files:**
- Modify: `tests/toolserver/test_tool_dispatcher.cpp`(追加 TEST_CASE)

**Interfaces:**
- Consumes: Task 1 `Fixture`、`time.advance`(Permission=Automation)。
- Produces: 无新增接口。

- [ ] **Step 1: 追加角色裁决测试**

`time.advance` 要求 `Automation`:`Agent` 应被拒、`Automation`/`Editor` 应放行。缺省 role(`Editor`)亦放行。

```cpp
TEST_CASE("ToolDispatcher:role 裁决 — Agent 调 time.advance(Automation)被拒") {
    Fixture f;
    const json out = json::parse(f.dispatcher.HandleInvoke(
        R"({"name":"time.advance","params":{"minutes":10},"role":"Agent"})"));
    CHECK(out["ok"] == false);
    CHECK(out["code"] == "PermissionDenied");
}

TEST_CASE("ToolDispatcher:role 裁决 — Automation 调 time.advance 放行") {
    Fixture f;
    const json out = json::parse(f.dispatcher.HandleInvoke(
        R"({"name":"time.advance","params":{"minutes":10},"role":"Automation"})"));
    CHECK(out["ok"] == true);
}

TEST_CASE("ToolDispatcher:role 缺省为 Editor — time.advance 放行") {
    Fixture f;
    const json out = json::parse(
        f.dispatcher.HandleInvoke(R"({"name":"time.advance","params":{"minutes":10}})"));
    CHECK(out["ok"] == true);
}
```

> 注:`time.advance` 参数名此处用 `minutes`;若 `engine/toolapi/src/tools/TimeTools.cpp` 用别名(如 `gameMinutes`),执行者据实改测试参数名,**不**改 Tool。

- [ ] **Step 2: 运行,确认通过**

Run: `cmake --build build-wsl --target me_tests -j 2>&1 | tail -3 && ./build-wsl/bin/me_tests -ts="ToolDispatcher*" 2>&1 | tail -3`
Expected: 12 个 ToolDispatcher 用例全 PASS。

- [ ] **Step 3: 提交**

```bash
git add tests/toolserver/test_tool_dispatcher.cpp
git commit -m "test(toolserver): role 解析裁决(Agent/Automation/缺省 Editor)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 4: HandleListTools(13 条元数据 + Schema)

**Files:**
- Modify: `engine/toolserver/src/ToolDispatcher.cpp`(实现 `HandleListTools` + 枚举字符串化助手)
- Modify: `tests/toolserver/test_tool_dispatcher.cpp`(追加 TEST_CASE)

**Interfaces:**
- Consumes: `ToolRegistry::ListNames()`、`ToolRegistry::Find(name)` → `const ITool*`(`name()`/`category()`/`permission()`/`paramsSchema()`)、`ToolCategory{Query,Mutation}`、`Permission{AgentAllowed,Automation,EditorOnly}`。
- Produces: `HandleListTools()` 返回 `[{name:string, category:string, permission:string, paramsSchema:object}]` 的 JSON 字符串。

- [ ] **Step 1: 写失败测试**

```cpp
TEST_CASE("ToolDispatcher:HandleListTools 返回 13 条带元数据") {
    Fixture f;
    const json tools = json::parse(f.dispatcher.HandleListTools());
    REQUIRE(tools.is_array());
    CHECK(tools.size() == 13);

    // 收集成 name → 条目,便于断言具体 Tool。
    json byName = json::object();
    for (const auto& t : tools) {
        REQUIRE(t.contains("name"));
        REQUIRE(t.contains("category"));
        REQUIRE(t.contains("permission"));
        REQUIRE(t.contains("paramsSchema"));
        byName[t["name"].get<std::string>()] = t;
    }

    REQUIRE(byName.contains("entity.set_transform"));
    CHECK(byName["entity.set_transform"]["category"] == "Mutation");
    CHECK(byName["entity.set_transform"]["permission"] == "Automation");
    // paramsSchema 是 JSON Schema 子集对象,含 required 列表。
    CHECK(byName["entity.set_transform"]["paramsSchema"]["type"] == "object");

    REQUIRE(byName.contains("scene.list_entities"));
    CHECK(byName["scene.list_entities"]["category"] == "Query");
    CHECK(byName["scene.list_entities"]["permission"] == "AgentAllowed");

    REQUIRE(byName.contains("scene.destroy_entity"));
    CHECK(byName["scene.destroy_entity"]["permission"] == "EditorOnly");
}
```

- [ ] **Step 2: 运行,确认失败**

Run: `cmake --build build-wsl --target me_tests -j 2>&1 | tail -3 && ./build-wsl/bin/me_tests -tc="*HandleListTools*" 2>&1 | tail -5`
Expected: FAIL(当前返回空数组 `size()==0`)。

- [ ] **Step 3: 实现 `HandleListTools` + 枚举助手**

在 `engine/toolserver/src/ToolDispatcher.cpp` 顶部增加 include 与匿名命名空间助手:

```cpp
#include "me/toolapi/ITool.h"   // 增:ToolCategory / ITool 元数据
```

在已有匿名 `namespace { ... }` 内,`ErrorJson` 之后追加:

```cpp
using me::toolapi::ITool;
using me::toolapi::Permission;
using me::toolapi::ToolCategory;

const char* CategoryToString(ToolCategory c) {
    switch (c) {
        case ToolCategory::Query:    return "Query";
        case ToolCategory::Mutation: return "Mutation";
    }
    return "Unknown"; // 穷尽 switch 后兜底
}

const char* PermissionToString(Permission p) {
    switch (p) {
        case Permission::AgentAllowed: return "AgentAllowed";
        case Permission::Automation:   return "Automation";
        case Permission::EditorOnly:   return "EditorOnly";
    }
    return "Unknown"; // 穷尽 switch 后兜底
}
```

把 `HandleListTools` 替换为:

```cpp
std::string ToolDispatcher::HandleListTools() {
    std::lock_guard<std::mutex> lock(mutex_);
    nlohmann::json arr = nlohmann::json::array();
    for (const std::string& name : registry_.ListNames()) {
        const me::toolapi::ITool* tool = registry_.Find(name);
        if (!tool) continue; // ListNames 与 Find 同源,理论不发生;防御
        arr.push_back({
            {"name", tool->name()},
            {"category", CategoryToString(tool->category())},
            {"permission", PermissionToString(tool->permission())},
            {"paramsSchema", tool->paramsSchema()},
        });
    }
    return arr.dump();
}
```

- [ ] **Step 4: 运行,确认通过**

Run: `cmake --build build-wsl --target me_tests -j 2>&1 | tail -3 && ./build-wsl/bin/me_tests -tc="*HandleListTools*" 2>&1 | tail -3`
Expected: PASS。

- [ ] **Step 5: 提交**

```bash
git add engine/toolserver/src/ToolDispatcher.cpp tests/toolserver/test_tool_dispatcher.cpp
git commit -m "feat(toolserver): HandleListTools 暴露 13 Tool 元数据 + paramsSchema

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 5: 域能力贯通(time.advance + crop 往返经 dispatcher)

**Files:**
- Modify: `tests/toolserver/test_tool_dispatcher.cpp`(追加 TEST_CASE)

**Interfaces:**
- Consumes: Task 1 `Fixture`、`time.advance`、`crop.plant`、`crop.get_field`。
- Produces: 无新增接口(证明 dispatcher 能驱动 time/crop 子系统)。

- [ ] **Step 1: 追加贯通测试**

```cpp
TEST_CASE("ToolDispatcher:time.advance 推进后 time.get 反映变化") {
    Fixture f;
    const json before = json::parse(f.dispatcher.HandleInvoke(R"({"name":"time.get"})"));
    REQUIRE(before["ok"] == true);
    const int beforeMinute = before["data"]["minuteOfDay"].get<int>();

    const json adv = json::parse(
        f.dispatcher.HandleInvoke(R"({"name":"time.advance","params":{"minutes":10}})"));
    REQUIRE(adv["ok"] == true);

    const json after = json::parse(f.dispatcher.HandleInvoke(R"({"name":"time.get"})"));
    REQUIRE(after["ok"] == true);
    CHECK(after["data"]["minuteOfDay"].get<int>() != beforeMinute);
}

TEST_CASE("ToolDispatcher:crop.plant 后 crop.get_field 可见") {
    Fixture f;
    const json plant = json::parse(f.dispatcher.HandleInvoke(
        R"({"name":"crop.plant","params":{"tileX":2,"tileY":3,"cropId":"parsnip"}})"));
    REQUIRE(plant["ok"] == true);

    const json field = json::parse(f.dispatcher.HandleInvoke(R"({"name":"crop.get_field"})"));
    REQUIRE(field["ok"] == true);
    REQUIRE(field["data"]["crops"].size() == 1);
    CHECK(field["data"]["crops"][0]["cropId"] == "parsnip");
}
```

> 注:字段名(`minuteOfDay`、`crops[].cropId`)沿用 M8.1/M8.2 Tool 既有结果;若不符,执行者据 `TimeTools.cpp`/`CropTools.cpp` 改断言,**不**改 Tool。

- [ ] **Step 2: 运行,确认通过**

Run: `cmake --build build-wsl --target me_tests -j 2>&1 | tail -3 && ./build-wsl/bin/me_tests -ts="ToolDispatcher*" 2>&1 | tail -3`
Expected: 全部 ToolDispatcher 用例 PASS(共 14 个)。

- [ ] **Step 3: 全量回归**

Run: `./build-wsl/bin/me_tests 2>&1 | tail -3`
Expected: `test cases: 207 | 207 passed`(193 + 14 新)。

- [ ] **Step 4: 提交**

```bash
git add tests/toolserver/test_tool_dispatcher.cpp
git commit -m "test(toolserver): 域能力贯通(time.advance + crop 往返经 dispatcher)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 6: cpp-httplib 依赖 + HttpToolServer 薄壳 + 无头 toolserver_app

**Files:**
- Modify: `third_party/CMakeLists.txt`(增 cpp-httplib FetchContent + INTERFACE target)
- Create: `engine/toolserver/include/me/toolserver/HttpToolServer.h`
- Create: `engine/toolserver/src/HttpToolServer.cpp`
- Modify: `engine/toolserver/CMakeLists.txt`(增源 + 链接 httplib + Threads)
- Create: `apps/toolserver/main.cpp`
- Create: `apps/toolserver/CMakeLists.txt`
- Modify: `CMakeLists.txt`(增 `add_subdirectory(apps/toolserver)`)

**Interfaces:**
- Consumes: `me::toolserver::ToolDispatcher`(`HandleInvoke`/`HandleListTools`)、`me::platform::ReadTextFile(path)` → `std::optional<std::string>`、`me::domain::LoadTimeConfig(json)`、`me::domain::LoadCropDatabase(json)`、`me::toolapi::RegisterBuiltinTools(registry)`、`ME_LOG_ERROR`/`ME_LOG_INFO`。
- Produces:
  - `me::toolserver::kBindHost`(`"127.0.0.1"`)、`kDefaultPort`(`8080`);
  - `class HttpToolServer { HttpToolServer(ToolDispatcher&, std::string host, int port, std::string staticRoot=""); bool Run(); void Stop(); }`;
  - 可执行 target `toolserver_app`。

- [ ] **Step 1: 增 cpp-httplib 到 `third_party/CMakeLists.txt`**

在文件末尾追加(单头 `httplib.h` 在仓库根,用 release tarball URL 避免全量 git 历史,沿用 nlohmann 教训):

```cmake
# cpp-httplib:单头 HTTP/JSON 服务库(M9.1 Tool 传输层)。
# 用 release tarball URL 而非 git clone,避免全量历史(沿用 nlohmann 教训)。
FetchContent_Declare(
    cpp_httplib
    URL https://github.com/yhirose/cpp-httplib/archive/refs/tags/v0.18.3.tar.gz
)
FetchContent_MakeAvailable(cpp_httplib)

# 单头封装:INTERFACE target,头文件 httplib.h 在解压根目录。
# 默认不启用 OpenSSL(无 HTTPS);需要线程库。
find_package(Threads REQUIRED)
add_library(cpp_httplib INTERFACE)
add_library(cpp_httplib::cpp_httplib ALIAS cpp_httplib)
target_include_directories(cpp_httplib INTERFACE ${cpp_httplib_SOURCE_DIR})
target_link_libraries(cpp_httplib INTERFACE Threads::Threads)
# 提供 target: cpp_httplib::cpp_httplib
```

- [ ] **Step 2: 写 `engine/toolserver/include/me/toolserver/HttpToolServer.h`**

```cpp
#pragma once

#include <memory>
#include <string>

namespace httplib { class Server; } // 前置声明:封死 httplib.h 在 .cpp 内

namespace me::toolserver {

class ToolDispatcher;

/// @brief 默认绑定主机(仅本地;本地开发工具,非生产服务)。
inline constexpr const char* kBindHost = "127.0.0.1";
/// @brief 默认端口(可由 toolserver_app 命令行覆盖)。
inline constexpr int kDefaultPort = 8080;

/**
 * @brief cpp-httplib 薄壳:HTTP 路由 → ToolDispatcher,逻辑全部委托 dispatcher。
 *
 * 路由:POST /invoke、GET /tools、可选 GET /*(静态根目录)。
 * 薄到无需自动化 socket 测试,手动 curl 冒烟(见 README)。
 */
class HttpToolServer {
public:
    /// @param dispatcher  逻辑核心(本壳不拥有,生命周期由调用方保证)。
    /// @param host        绑定地址(如 kBindHost)。
    /// @param port        监听端口。
    /// @param staticRoot  可选静态文件根目录;空则禁用静态服务。
    HttpToolServer(ToolDispatcher& dispatcher, std::string host, int port,
                   std::string staticRoot = {});
    ~HttpToolServer(); // 定义在 .cpp:unique_ptr<httplib::Server> 需完整类型

    HttpToolServer(const HttpToolServer&) = delete;
    HttpToolServer& operator=(const HttpToolServer&) = delete;

    /// @brief 注册三路由并阻塞 listen();绑定失败返回 false。
    bool Run();
    /// @brief 线程安全地停止 listen()(供信号处理调用)。
    void Stop();

private:
    ToolDispatcher& dispatcher_;
    std::string host_;
    int port_;
    std::string staticRoot_;
    std::unique_ptr<httplib::Server> server_;
};

} // namespace me::toolserver
```

- [ ] **Step 3: 写 `engine/toolserver/src/HttpToolServer.cpp`**

```cpp
#include "me/toolserver/HttpToolServer.h"

#include <httplib.h>

#include "me/toolserver/ToolDispatcher.h"

namespace me::toolserver {

namespace {
constexpr const char* kJsonMime = "application/json";
} // namespace

HttpToolServer::HttpToolServer(ToolDispatcher& dispatcher, std::string host, int port,
                               std::string staticRoot)
    : dispatcher_(dispatcher),
      host_(std::move(host)),
      port_(port),
      staticRoot_(std::move(staticRoot)) {}

HttpToolServer::~HttpToolServer() = default;

bool HttpToolServer::Run() {
    server_ = std::make_unique<httplib::Server>();

    server_->Post("/invoke", [this](const httplib::Request& req, httplib::Response& res) {
        res.set_content(dispatcher_.HandleInvoke(req.body), kJsonMime);
    });
    server_->Get("/tools", [this](const httplib::Request&, httplib::Response& res) {
        res.set_content(dispatcher_.HandleListTools(), kJsonMime);
    });
    if (!staticRoot_.empty()) {
        // 同源服务前端静态资源(免 CORS);失败不致命(端点仍可用)。
        server_->set_mount_point("/", staticRoot_);
    }

    return server_->listen(host_, port_); // 阻塞直到 Stop()
}

void HttpToolServer::Stop() {
    if (server_) server_->stop(); // httplib::Server::stop 线程安全
}

} // namespace me::toolserver
```

- [ ] **Step 4: 更新 `engine/toolserver/CMakeLists.txt`**

```cmake
add_library(me_toolserver STATIC
    src/ToolDispatcher.cpp
    src/HttpToolServer.cpp
)
target_include_directories(me_toolserver PUBLIC include)
target_compile_features(me_toolserver PUBLIC cxx_std_17)
# 单向依赖:toolserver → toolapi → command → scene → core,以及 toolapi/domain。
# cpp-httplib 仅用于 HttpToolServer.cpp;PUBLIC 以便 me_tests 链接本静态库时
# 自动获得 Threads(HttpToolServer.o 引用 pthread 符号)。
target_link_libraries(me_toolserver PUBLIC
    me_toolapi me_domain me_scene me_command me_core
    nlohmann_json::nlohmann_json cpp_httplib::cpp_httplib)
```

- [ ] **Step 5: 写 `apps/toolserver/main.cpp`**

```cpp
// MiniEngine 无头 Tool 服务器:加载 time/crops 配置,组装受控上下文,跑本地 HTTP 服务。
// 不链接 RHI/DX12;场景以空场景启动,经 scene.create_entity 编辑(见计划范围说明)。
#include <atomic>
#include <csignal>
#include <cstdlib>
#include <string>

#include <nlohmann/json.hpp>

#include "me/command/CommandStack.h"
#include "me/core/Log.h"
#include "me/domain/CropConfig.h"
#include "me/domain/FarmField.h"
#include "me/domain/TimeConfig.h"
#include "me/domain/TimeSystem.h"
#include "me/platform/FileSystem.h"
#include "me/scene/Scene.h"
#include "me/toolapi/ToolContext.h"
#include "me/toolapi/ToolRegistry.h"
#include "me/toolapi/tools/BuiltinTools.h"
#include "me/toolserver/HttpToolServer.h"
#include "me/toolserver/ToolDispatcher.h"

namespace {

me::toolserver::HttpToolServer* g_server = nullptr; ///< 供信号处理停止(进程级唯一,生命周期覆盖 listen)

void HandleSigint(int) {
    if (g_server) g_server->Stop();
}

/// @brief 读文件并解析为 JSON;失败打错误日志返回 nullopt。
std::optional<nlohmann::json> LoadJsonFile(const std::string& path) {
    const auto text = me::platform::ReadTextFile(path);
    if (!text) {
        ME_LOG_ERROR("无法读取配置文件: %s", path.c_str());
        return std::nullopt;
    }
    auto j = nlohmann::json::parse(*text, nullptr, /*allow_exceptions=*/false);
    if (j.is_discarded()) {
        ME_LOG_ERROR("配置文件非法 JSON: %s", path.c_str());
        return std::nullopt;
    }
    return j;
}

} // namespace

int main(int argc, char** argv) {
    namespace dom = me::domain;
    namespace api = me::toolapi;
    namespace srv = me::toolserver;

    const std::string assetDir = ME_ASSET_DIR; // 编译期注入(见 apps/toolserver/CMakeLists.txt)
    int port = srv::kDefaultPort;
    if (argc >= 2) {
        port = std::atoi(argv[1]); // 可选命令行覆盖端口
        if (port <= 0) { ME_LOG_ERROR("非法端口: %s", argv[1]); return 1; }
    }
    const std::string staticRoot = (argc >= 3) ? argv[2] : std::string{}; // 可选前端静态根

    // —— 加载 mandatory 配置:缺失/非法即退出(无它们 time/crop Tool 无法工作)——
    const auto timeJson = LoadJsonFile(assetDir + "/config/time.json");
    if (!timeJson) return 1;
    const auto timeCfg = dom::LoadTimeConfig(*timeJson);
    if (!timeCfg) { ME_LOG_ERROR("time.json 语义校验失败"); return 1; }

    const auto cropJson = LoadJsonFile(assetDir + "/config/crops.json");
    if (!cropJson) return 1;
    auto cropDb = dom::LoadCropDatabase(*cropJson);
    if (!cropDb) { ME_LOG_ERROR("crops.json 语义校验失败"); return 1; }

    // —— 组装受控状态(空场景:前端经 scene.create_entity 编辑)——
    me::scene::Scene scene;
    me::command::CommandStack stack;
    api::ToolInvocationLog log;
    dom::TimeSystem time(*timeCfg);
    dom::FarmField farm(*cropDb);
    api::ToolContext ctx{scene, stack, log, &time, &farm};

    api::ToolRegistry registry;
    api::RegisterBuiltinTools(registry);

    srv::ToolDispatcher dispatcher(ctx, registry);
    srv::HttpToolServer server(dispatcher, srv::kBindHost, port, staticRoot);
    g_server = &server;
    std::signal(SIGINT, HandleSigint);

    ME_LOG_INFO("Tool 服务器监听 http://%s:%d (Ctrl+C 退出)", srv::kBindHost, port);
    if (!server.Run()) { // 阻塞
        ME_LOG_ERROR("监听失败(端口 %d 可能被占用)", port);
        return 1;
    }
    ME_LOG_INFO("Tool 服务器已停止");
    return 0;
}
```

> 注:`ME_LOG_*` 的格式化签名以 `engine/core/include/me/core/Log.h` 为准;若它不支持 printf 风格变参,执行者改用其实际接口(如流式 / `std::string` 拼接),**不**改 Log。

- [ ] **Step 6: 写 `apps/toolserver/CMakeLists.txt`**

```cmake
add_executable(toolserver_app main.cpp)
target_compile_features(toolserver_app PRIVATE cxx_std_17)
target_link_libraries(toolserver_app PRIVATE
    me_toolserver me_toolapi me_domain me_scene me_command me_platform me_core
    nlohmann_json::nlohmann_json)
# 无头:不链接 me_rhi/me_renderer/imgui。资产根经编译定义注入(me_rhi 的 ME_ASSET_DIR
# 是 Windows-only,本可执行跨平台,故自行定义)。
target_compile_definitions(toolserver_app PRIVATE ME_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets")
```

- [ ] **Step 7: 接入顶层构建 `CMakeLists.txt`**

在 `if(ME_BUILD_TESTS) ... endif()` 之后、`if(WIN32) add_subdirectory(sandbox) endif()` 之前新增(无头,跨平台,不 WIN32-gate):

```cmake
# 无头 Tool 服务器(HTTP+JSON 传输层),跨平台,不依赖 DX12。
add_subdirectory(apps/toolserver)
```

- [ ] **Step 8: 配置 + 全量构建,确认编译链接通过**

Run: `cmake -S . -B build-wsl -DME_BUILD_TESTS=ON >/dev/null && cmake --build build-wsl -j 2>&1 | tail -8`
Expected: `me_toolserver`、`toolserver_app`、`me_tests` 均编译链接成功,无 error。

- [ ] **Step 9: 全量回归(确认链接 httplib 后旧测试不回归)**

Run: `./build-wsl/bin/me_tests 2>&1 | tail -3`
Expected: `test cases: 207 | 207 passed`。

- [ ] **Step 10: 手动 curl 冒烟(后台起服务 → 调用 → 停)**

Run:
```bash
./build-wsl/bin/toolserver_app >/tmp/toolserver.log 2>&1 &
SRV_PID=$!; sleep 1
curl -s -XPOST localhost:8080/invoke -d '{"name":"scene.create_entity"}'; echo
curl -s localhost:8080/invoke -XPOST -d '{"name":"scene.list_entities"}'; echo
curl -s localhost:8080/tools | head -c 200; echo
kill $SRV_PID
```
Expected: 第 1 行 `{"ok":true,...,"invocationId":1}`;第 2 行 `data.entities` 含 1 个实体;第 3 行 `/tools` 输出以 `[{"name":...` 开头的 JSON 数组。

- [ ] **Step 11: 提交**

```bash
git add third_party/CMakeLists.txt engine/toolserver apps/toolserver CMakeLists.txt
git commit -m "feat(toolserver): cpp-httplib HttpToolServer 薄壳 + 无头 toolserver_app

三端点 POST /invoke、GET /tools、GET /*(静态);仅绑 127.0.0.1。
mandatory time/crops 配置缺失即退出;空场景启动。curl 冒烟通过。

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

### Task 7: 文档(ADR 0008 + 模块 README + spec 状态 + PROGRESS 回写)

**Files:**
- Create: `docs/architecture/0008-m9-1-toolserver-http-transport.md`
- Create: `engine/toolserver/README.md`
- Modify: `docs/superpowers/specs/2026-06-22-toolserver-http-transport-design.md`(状态行)
- Modify: `docs/PROGRESS.md`(里程碑表 + 现状 + 下一步 + 文档索引 + ADR 摘要)

**Interfaces:**
- Consumes: 无(纯文档)。
- Produces: 无(纯文档)。

- [ ] **Step 1: 写 ADR `docs/architecture/0008-m9-1-toolserver-http-transport.md`**

内容覆盖 spec §8 的 8 条关键决策,逐条「决策 / 理由」,并记录本切片的实现取舍:
- Tool API 暴露为本地 HTTP+JSON = Agent-ready 传输层首切(人类 UI 与未来 agent 同一受控边界)。
- 无头 Tool 服务器(非嵌入 DX12),DX12 sandbox 原样保留为试玩。
- cpp-httplib 单头 + FetchContent 钉版 v0.18.3;其内部用线程/异常属第三方,不违反「我方代码不用异常」。
- ToolDispatcher(纯逻辑、可 doctest)/ HttpToolServer(socket 薄壳、封死边界)剖分,延续 M7。
- 串行互斥锁:每请求一线程下串行化 Tool 调用,保证共享 Scene/Farm 原子。
- 业务错误走 ToolResult + HTTP 200;HTTP 状态码只表达协议级问题。
- 仅绑 127.0.0.1、默认 Editor 全权;鉴权留后续。
- GET /tools 暴露 Tool 元数据(name/category/permission/paramsSchema),前端数据驱动生成表单/按权限灰按钮。
- **实现取舍**:本切片不做 tmj→Scene 实体加载器(当前无此能力);`toolserver_app` 空场景启动,time/crops 配置 mandatory 失败退出;场景种子留后续切片。

- [ ] **Step 2: 写模块 `engine/toolserver/README.md`**

含:模块职责(ToolDispatcher / HttpToolServer 剖分)、依赖图、HTTP 协议三端点表(对齐 spec §3)、运行方式(`./build-wsl/bin/toolserver_app [port] [staticRoot]`)、curl 冒烟示例(同 Task 6 Step 10)、与前端 `tools/editor-frontend`(把 `invoke()`/`listTools()` 换成 `fetch http://127.0.0.1:8080`)的衔接说明、安全边界(仅本地、无鉴权)。

- [ ] **Step 3: 翻转 spec 状态**

把 `docs/superpowers/specs/2026-06-22-toolserver-http-transport-design.md` 第 5 行
`- **状态**:已确认,待写实现计划`
改为
`- **状态**:已实现(M9.1,见 plans/2026-06-23-m9-1-toolserver-http-transport.md + ADR 0008)`。

- [ ] **Step 4: 回写 `docs/PROGRESS.md`**

- 顶部「最后更新」改 `2026-06-23`;「当前阶段」改为 M9.1 Tool 传输层完成(me_toolserver + ToolDispatcher + HttpToolServer + toolserver_app,WSL 207/207 全绿),下一步=网页编辑器前端接线 / M8.3 库存。
- 「一句话现状」末尾追加 M9.1 段落(模块、三端点、剖分、串行锁、curl 冒烟、空场景取舍、207/207)。
- 里程碑表:`M9+ 未来` 行拆出 `M9.1 Tool HTTP 传输层 ☑`。
- 文档索引表增 3 行(本 spec、本 plan、ADR 0008)。
- 「下一步行动」更新:M9.1 标 ☑;新增「网页编辑器前端接线(把 mock 传输换 fetch)」与延后的 M8.3 库存。
- ADR 摘要表追加 2026-06-23 行(无头 HTTP 传输层、ToolDispatcher/HttpToolServer 剖分、串行锁、cpp-httplib 钉版)。

- [ ] **Step 5: 提交**

```bash
git add docs/architecture/0008-m9-1-toolserver-http-transport.md engine/toolserver/README.md docs/superpowers/specs/2026-06-22-toolserver-http-transport-design.md docs/PROGRESS.md
git commit -m "docs(M9.1): ADR 0008 + 模块 README + spec 状态 + PROGRESS 回写

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Self-Review

**Spec coverage:**
- §1 做:cpp-httplib(Task 6)、me_toolserver/ToolDispatcher/HttpToolServer(Task 1/4/6)、toolserver_app(Task 6)、三端点(Task 6)、ToolDispatcher 全 CPU doctest(Task 1–5)。✅
- §1 不做/不动:无 SSE/WS、无鉴权、不动 sandbox —— 计划未触及,符合。✅
- §2 架构 + 模块依赖:Task 1/6 CMake 单向链接。✅
- §3 HTTP 协议三端点 + 约定(role 缺省 Editor、params/dryRun 缺省、业务错误 HTTP 200、code 稳定字符串):Task 1(invoke 约定)、Task 4(/tools)、Task 6(路由 + 静态)。✅
- §4 ToolDispatcher 接口:Task 1 头文件逐字对齐;§4 注「registry 枚举」用既有 `ListNames`+`Find` 满足,无需新增 registry 方法。✅
- §5 并发/生命周期:互斥锁(Task 1)、SIGINT→Stop(Task 6)、无后台 tick(显式 time.advance/crop.advance_days)。✅
- §6 资产/安全/边界:mandatory 配置失败退出、场景缺失→空场景(Task 6,且因无 tmj→Scene 加载器统一为空场景启动,已在范围说明声明)、仅绑 127.0.0.1、具名端口常量。✅
- §7 测试策略 6 组:合法 invoke(T1)、dryRun(T1)、错误路径(T2)、role 解析(T3)、HandleListTools(T4)、域贯通(T5)、curl 冒烟(T6)。✅
- §8 关键决策 → ADR 0008(Task 7)。✅

**Placeholder scan:** 每个代码步给出完整代码;每个命令步给出确切命令 + 预期输出;无 TBD/TODO/“类似 Task N”。字段名不确定处统一加「据实改测试断言、不改 Tool」的明确指示(非占位,是防御性校准约定)。✅

**Type consistency:** `ToolDispatcher(ToolContext&, ToolRegistry&)`、`HandleInvoke(const std::string&)→std::string`、`HandleListTools()→std::string`、`HttpToolServer(ToolDispatcher&, std::string, int, std::string)`、`kBindHost`/`kDefaultPort`、`CategoryToString`/`PermissionToString`/`ParseRole` 全程一致;`ToolResult::toJson()` 字段 `{ok,code,message,data,invocationId}` 与测试断言一致。✅
