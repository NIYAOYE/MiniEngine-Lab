# M0 地基 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 搭起 MiniEngine 的工程骨架,实现可独立单测的 Core(2D 数学 / 日志 / 断言 / 句柄)与 Platform(计时 / 文件系统),为 M1 起的 RHI/渲染打好底座。

**Architecture:** 模块化 CMake,每个引擎模块编译为一个 `STATIC` 库 target,依赖严格单向(`me_platform` → `me_core`,`me_core` 零依赖),由构建图强制。本里程碑全部代码跨平台(标准库 + `<chrono>`/`<filesystem>`),可在 WSL2(GCC/Clang)上 `cmake --build` + `ctest` 跑通;Win32 Window/Input 推迟到 M1。数学库采用**行向量 + 行主序**约定(`p' = p * M`),与 DX12/DirectXMath 同源。

**Tech Stack:** C++17、CMake ≥ 3.20、doctest(经 `FetchContent`,M0 唯一第三方依赖)。

## Global Constraints

逐条来自 spec 与 CLAUDE.md,**每个 Task 都隐含遵守**:

- 语言标准:**C++17**(`CMAKE_CXX_STANDARD 17`,`CXX_STANDARD_REQUIRED ON`,`CXX_EXTENSIONS OFF`)。
- 构建系统:**CMake ≥ 3.20**;每模块一个 `STATIC` 库 target;依赖**单向**,由构建图强制。
- 命名空间:Core 用 `me`,Platform 用 `me::platform`;**头文件禁止 `using namespace std`**。
- 命名:`PascalCase` 类型/函数,`m_camelCase` 私有成员(公开 POD 字段如 `x/y` 保持原样),`s_` 静态,`g_` 全局;所有头文件 `#pragma once`。
- 错误处理:**不使用 C++ 异常**。可恢复错误用返回值 / `std::optional`;不变量违反用 `ME_ASSERT`。文件加载等失败必须显式返回,不静默忽略。
- **零魔法数字**:所有数值常量是具名 `constexpr` 或来自配置。
- **数据驱动 / 无硬编码**:源码中不出现游戏数值或内容字符串(M0 无内容,但约定生效)。
- 架构禁令:禁 Singleton、禁全局可变状态、依赖注入。
- 注释:每个公开 API 函数写 Doxygen 注释(`/** ... */`);非显而易见实现写行内注释。
- 内存:优先 RAII / 标准库容器。
- 第三方:M0 仅引入 **doctest**;放 `third_party/`,用 CMake `FetchContent`。

**目录与包含路径约定(全程一致):**

```
MiniEngine/
├─ CMakeLists.txt                      # 根:project + 选项 + add_subdirectory
├─ third_party/CMakeLists.txt          # FetchContent doctest
├─ engine/
│  ├─ core/
│  │  ├─ CMakeLists.txt                # me_core STATIC
│  │  ├─ include/me/core/*.h           # 对外头(包含为 "me/core/X.h")
│  │  └─ src/*.cpp
│  └─ platform/
│     ├─ CMakeLists.txt                # me_platform STATIC, links me_core
│     ├─ include/me/platform/*.h       # 包含为 "me/platform/X.h"
│     └─ src/*.cpp
└─ tests/
   ├─ CMakeLists.txt                   # me_tests 可执行,链 doctest + me_core + me_platform
   ├─ test_main.cpp                    # 唯一定义 doctest main 的 TU
   ├─ core/*.cpp
   └─ platform/*.cpp
```

- 头文件包含一律写全路径:`#include "me/core/Vector2.h"`、`#include "me/platform/Time.h"`。
- 每个模块 `target_include_directories(<tgt> PUBLIC include)`,头放在 `include/me/<module>/` 下。
- 浮点相等断言一律用 `doctest::Approx`。

---

### Task 1: 工程骨架 + CMake + doctest 冒烟测试

把构建系统、第三方依赖、空的 Core/Platform 库、测试可执行体全部接通,跑通一个 trivial 测试。这是后续所有 TDD 的地基,所以把全部脚手架折叠进本 Task。

**Files:**
- Create: `CMakeLists.txt`
- Create: `third_party/CMakeLists.txt`
- Create: `engine/core/CMakeLists.txt`
- Create: `engine/core/include/me/core/Version.h`
- Create: `engine/core/src/Version.cpp`
- Create: `engine/platform/CMakeLists.txt`
- Create: `engine/platform/include/me/platform/Platform.h`
- Create: `engine/platform/src/Platform.cpp`
- Create: `tests/CMakeLists.txt`
- Create: `tests/test_main.cpp`
- Create: `tests/core/test_smoke.cpp`
- Create: `.gitignore`(追加 build 目录忽略)

**Interfaces:**
- Consumes: 无(首个 Task)。
- Produces:
  - CMake target `me_core`(STATIC,`PUBLIC` 包含 `engine/core/include`)。
  - CMake target `me_platform`(STATIC,`PUBLIC` 链接 `me_core`,`PUBLIC` 包含 `engine/platform/include`)。
  - CMake target `me_tests`(可执行,链 `doctest::doctest`、`me_core`、`me_platform`),并注册为 CTest 测试 `me_tests`。
  - 函数 `const char* me::EngineName();` 返回 `"MiniEngine"`。

- [ ] **Step 1: 写根 CMakeLists.txt**

```cmake
cmake_minimum_required(VERSION 3.20)
project(MiniEngine LANGUAGES CXX)

# ---- 全局 C++ 标准(见 Global Constraints)----
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- 选项 ----
option(ME_BUILD_TESTS "Build unit tests" ON)

# 单一构建输出目录,便于查找产物
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_subdirectory(third_party)
add_subdirectory(engine/core)
add_subdirectory(engine/platform)

if(ME_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
```

- [ ] **Step 2: 写 third_party/CMakeLists.txt(引入 doctest)**

```cmake
# M0 唯一第三方依赖:doctest(单头单测框架)
include(FetchContent)

FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG        v2.4.11
)
FetchContent_MakeAvailable(doctest)
# 提供 target: doctest::doctest
```

- [ ] **Step 3: 写 engine/core/CMakeLists.txt**

```cmake
add_library(me_core STATIC
    src/Version.cpp
)

target_include_directories(me_core PUBLIC include)
target_compile_features(me_core PUBLIC cxx_std_17)
```

- [ ] **Step 4: 写 Core 占位头与实现**

`engine/core/include/me/core/Version.h`:
```cpp
#pragma once

namespace me {

/** @brief 返回引擎名称常量字符串。用于冒烟验证链路接通。 */
const char* EngineName();

} // namespace me
```

`engine/core/src/Version.cpp`:
```cpp
#include "me/core/Version.h"

namespace me {

namespace {
// 引擎名称常量(零魔法数字/字符串:具名常量集中存放)。
constexpr const char* kEngineName = "MiniEngine";
} // namespace

const char* EngineName() {
    return kEngineName;
}

} // namespace me
```

- [ ] **Step 5: 写 engine/platform/CMakeLists.txt**

```cmake
add_library(me_platform STATIC
    src/Platform.cpp
)

target_include_directories(me_platform PUBLIC include)
target_compile_features(me_platform PUBLIC cxx_std_17)

# 单向依赖:platform → core(core 不得反向依赖 platform)
target_link_libraries(me_platform PUBLIC me_core)
```

- [ ] **Step 6: 写 Platform 占位头与实现**

`engine/platform/include/me/platform/Platform.h`:
```cpp
#pragma once

namespace me::platform {

/** @brief 返回当前构建的目标平台名("Windows"/"Linux"/"Unknown")。 */
const char* PlatformName();

} // namespace me::platform
```

`engine/platform/src/Platform.cpp`:
```cpp
#include "me/platform/Platform.h"

namespace me::platform {

const char* PlatformName() {
#if defined(_WIN32)
    return "Windows";
#elif defined(__linux__)
    return "Linux";
#else
    return "Unknown";
#endif
}

} // namespace me::platform
```

- [ ] **Step 7: 写 tests/CMakeLists.txt**

```cmake
add_executable(me_tests
    test_main.cpp
    core/test_smoke.cpp
)

target_link_libraries(me_tests PRIVATE doctest::doctest me_core me_platform)

add_test(NAME me_tests COMMAND me_tests)
```

- [ ] **Step 8: 写 doctest 主入口(唯一提供 main 的 TU)**

`tests/test_main.cpp`:
```cpp
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
```

- [ ] **Step 9: 写冒烟测试**

`tests/core/test_smoke.cpp`:
```cpp
#include <doctest/doctest.h>

#include "me/core/Version.h"
#include "me/platform/Platform.h"

#include <cstring>

TEST_CASE("engine name is MiniEngine") {
    CHECK(std::strcmp(me::EngineName(), "MiniEngine") == 0);
}

TEST_CASE("platform name is non-empty") {
    CHECK(std::strlen(me::platform::PlatformName()) > 0);
}
```

- [ ] **Step 10: 追加 .gitignore**

确保 `.gitignore` 含(若已存在内容则在末尾追加,不要删原内容):
```
/build/
```

- [ ] **Step 11: 配置 + 构建 + 跑测试,验证全链路**

Run:
```bash
cmake -S . -B build -DME_BUILD_TESTS=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
```
Expected: 配置成功(首次会用 git 拉取 doctest);编译无错误;`ctest` 输出 `100% tests passed, 0 tests failed out of 1`,两个 TEST_CASE 全 PASS。

- [ ] **Step 12: 提交**

```bash
git add CMakeLists.txt third_party engine tests .gitignore
git commit -m "build(m0): 工程骨架 + CMake 模块化 + doctest 冒烟测试"
```

---

### Task 2: Core 数学常量 + Vector2

**Files:**
- Create: `engine/core/include/me/core/MathConstants.h`
- Create: `engine/core/include/me/core/Vector2.h`
- Create: `engine/core/src/Vector2.cpp`
- Modify: `engine/core/CMakeLists.txt`(加 `src/Vector2.cpp`)
- Modify: `tests/CMakeLists.txt`(加 `core/test_vector2.cpp`)
- Test: `tests/core/test_vector2.cpp`

**Interfaces:**
- Consumes: 无。
- Produces:
  - `engine/core/include/me/core/MathConstants.h`:`constexpr float me::kPi`、`kTwoPi`、`kEpsilon`、`kDegToRad`、`kRadToDeg`。
  - `struct me::Vector2 { float x; float y; }`,含构造、`+ - * / -(一元)`、`+= -= *=`、`== !=`、`LengthSquared()`、`Length()`、`Normalized()`;自由函数 `float me::Dot(const Vector2&, const Vector2&)`。

- [ ] **Step 1: 写失败测试**

`tests/core/test_vector2.cpp`:
```cpp
#include <doctest/doctest.h>

#include "me/core/Vector2.h"
#include "me/core/MathConstants.h"

using me::Vector2;

TEST_CASE("Vector2 default is zero") {
    Vector2 v;
    CHECK(v.x == doctest::Approx(0.0f));
    CHECK(v.y == doctest::Approx(0.0f));
}

TEST_CASE("Vector2 arithmetic") {
    Vector2 a{1.0f, 2.0f};
    Vector2 b{3.0f, 4.0f};
    CHECK((a + b) == Vector2{4.0f, 6.0f});
    CHECK((b - a) == Vector2{2.0f, 2.0f});
    CHECK((a * 2.0f) == Vector2{2.0f, 4.0f});
    CHECK((b / 2.0f) == Vector2{1.5f, 2.0f});
    CHECK((-a) == Vector2{-1.0f, -2.0f});
}

TEST_CASE("Vector2 compound assignment") {
    Vector2 a{1.0f, 1.0f};
    a += Vector2{2.0f, 3.0f};
    CHECK(a == Vector2{3.0f, 4.0f});
    a -= Vector2{1.0f, 1.0f};
    CHECK(a == Vector2{2.0f, 3.0f});
    a *= 2.0f;
    CHECK(a == Vector2{4.0f, 6.0f});
}

TEST_CASE("Vector2 length and dot") {
    Vector2 v{3.0f, 4.0f};
    CHECK(v.LengthSquared() == doctest::Approx(25.0f));
    CHECK(v.Length() == doctest::Approx(5.0f));
    CHECK(me::Dot(Vector2{1.0f, 0.0f}, Vector2{0.0f, 1.0f}) == doctest::Approx(0.0f));
    CHECK(me::Dot(Vector2{2.0f, 3.0f}, Vector2{4.0f, 5.0f}) == doctest::Approx(23.0f));
}

TEST_CASE("Vector2 normalized") {
    Vector2 n = Vector2{0.0f, 5.0f}.Normalized();
    CHECK(n.x == doctest::Approx(0.0f));
    CHECK(n.y == doctest::Approx(1.0f));
    // 零向量归一化返回零向量(不除零)
    Vector2 z = Vector2{0.0f, 0.0f}.Normalized();
    CHECK(z == Vector2{0.0f, 0.0f});
}
```

加测试源到 `tests/CMakeLists.txt` 的 `add_executable(me_tests ...)` 列表:`core/test_vector2.cpp`。

- [ ] **Step 2: 跑测试确认失败**

Run: `cmake --build build -j 2>&1 | head -40`
Expected: 编译失败,找不到 `me/core/Vector2.h` / `me/core/MathConstants.h`。

- [ ] **Step 3: 写 MathConstants.h**

```cpp
#pragma once

namespace me {

/// 数学常量(零魔法数字:全部具名 constexpr)。
constexpr float kPi       = 3.14159265358979323846f;
constexpr float kTwoPi    = 2.0f * kPi;
constexpr float kEpsilon  = 1e-6f;              ///< 浮点近零阈值
constexpr float kDegToRad = kPi / 180.0f;
constexpr float kRadToDeg = 180.0f / kPi;

} // namespace me
```

- [ ] **Step 4: 写 Vector2.h**

```cpp
#pragma once

namespace me {

/**
 * @brief 二维向量(引擎主力数学类型)。
 *
 * 公开字段 x/y 为 POD,直接访问。约定世界空间 Y 轴向上。
 */
struct Vector2 {
    float x = 0.0f;
    float y = 0.0f;

    constexpr Vector2() = default;
    constexpr Vector2(float xIn, float yIn) : x(xIn), y(yIn) {}

    Vector2 operator+(const Vector2& r) const { return {x + r.x, y + r.y}; }
    Vector2 operator-(const Vector2& r) const { return {x - r.x, y - r.y}; }
    Vector2 operator*(float s) const { return {x * s, y * s}; }
    Vector2 operator/(float s) const { return {x / s, y / s}; }
    Vector2 operator-() const { return {-x, -y}; }

    Vector2& operator+=(const Vector2& r) { x += r.x; y += r.y; return *this; }
    Vector2& operator-=(const Vector2& r) { x -= r.x; y -= r.y; return *this; }
    Vector2& operator*=(float s) { x *= s; y *= s; return *this; }

    bool operator==(const Vector2& r) const { return x == r.x && y == r.y; }
    bool operator!=(const Vector2& r) const { return !(*this == r); }

    /** @brief 长度平方(避免开方,用于比较)。 */
    float LengthSquared() const { return x * x + y * y; }
    /** @brief 欧氏长度。 */
    float Length() const;
    /** @brief 返回单位向量;长度近零时返回零向量(不除零)。 */
    Vector2 Normalized() const;
};

/** @brief 点积。 */
inline float Dot(const Vector2& a, const Vector2& b) { return a.x * b.x + a.y * b.y; }

} // namespace me
```

- [ ] **Step 5: 写 Vector2.cpp**

```cpp
#include "me/core/Vector2.h"
#include "me/core/MathConstants.h"

#include <cmath>

namespace me {

float Vector2::Length() const {
    return std::sqrt(LengthSquared());
}

Vector2 Vector2::Normalized() const {
    const float len = Length();
    if (len <= kEpsilon) {
        return Vector2{0.0f, 0.0f}; // 防除零:零向量归一化仍为零向量
    }
    const float inv = 1.0f / len;
    return Vector2{x * inv, y * inv};
}

} // namespace me
```

加 `src/Vector2.cpp` 到 `engine/core/CMakeLists.txt`。

- [ ] **Step 6: 跑测试确认通过**

Run: `cmake --build build -j && ctest --test-dir build --output-on-failure`
Expected: 全部 PASS。

- [ ] **Step 7: 提交**

```bash
git add engine/core tests
git commit -m "feat(core): 数学常量 + Vector2"
```

---

### Task 3: Core Vector3 与 Vector4

供颜色 / 齐次坐标使用,接口比 Vector2 精简(YAGNI:只放当前需要的算术与点积)。

**Files:**
- Create: `engine/core/include/me/core/Vector3.h`
- Create: `engine/core/src/Vector3.cpp`
- Create: `engine/core/include/me/core/Vector4.h`
- Modify: `engine/core/CMakeLists.txt`(加 `src/Vector3.cpp`)
- Modify: `tests/CMakeLists.txt`(加 `core/test_vector34.cpp`)
- Test: `tests/core/test_vector34.cpp`

**Interfaces:**
- Consumes: 无。
- Produces:
  - `struct me::Vector3 { float x,y,z; }`:构造、`+ - *(标量)`、`==`、`LengthSquared()`、`Length()`;`float me::Dot(const Vector3&, const Vector3&)`。
  - `struct me::Vector4 { float x,y,z,w; }`:构造、`+ - *(标量)`、`==`(纯数据,主要作颜色/齐次)。

- [ ] **Step 1: 写失败测试**

`tests/core/test_vector34.cpp`:
```cpp
#include <doctest/doctest.h>

#include "me/core/Vector3.h"
#include "me/core/Vector4.h"

using me::Vector3;
using me::Vector4;

TEST_CASE("Vector3 basics") {
    Vector3 a{1.0f, 2.0f, 2.0f};
    CHECK(a.LengthSquared() == doctest::Approx(9.0f));
    CHECK(a.Length() == doctest::Approx(3.0f));
    CHECK((a + Vector3{1.0f, 0.0f, 0.0f}) == Vector3{2.0f, 2.0f, 2.0f});
    CHECK((a * 2.0f) == Vector3{2.0f, 4.0f, 4.0f});
    CHECK(me::Dot(Vector3{1.0f, 2.0f, 3.0f}, Vector3{4.0f, 5.0f, 6.0f}) == doctest::Approx(32.0f));
}

TEST_CASE("Vector4 basics") {
    Vector4 c{1.0f, 0.5f, 0.25f, 1.0f};
    CHECK((c + Vector4{0.0f, 0.5f, 0.0f, 0.0f}) == Vector4{1.0f, 1.0f, 0.25f, 1.0f});
    CHECK((c * 2.0f) == Vector4{2.0f, 1.0f, 0.5f, 2.0f});
    CHECK(c == Vector4{1.0f, 0.5f, 0.25f, 1.0f});
}
```
加 `core/test_vector34.cpp` 到 `tests/CMakeLists.txt`。

- [ ] **Step 2: 跑测试确认失败**

Run: `cmake --build build -j 2>&1 | head -30`
Expected: 找不到 `me/core/Vector3.h` / `Vector4.h`,编译失败。

- [ ] **Step 3: 写 Vector3.h**

```cpp
#pragma once

namespace me {

/** @brief 三维向量(用于齐次/方向;颜色亦可)。 */
struct Vector3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    constexpr Vector3() = default;
    constexpr Vector3(float xIn, float yIn, float zIn) : x(xIn), y(yIn), z(zIn) {}

    Vector3 operator+(const Vector3& r) const { return {x + r.x, y + r.y, z + r.z}; }
    Vector3 operator-(const Vector3& r) const { return {x - r.x, y - r.y, z - r.z}; }
    Vector3 operator*(float s) const { return {x * s, y * s, z * s}; }
    bool operator==(const Vector3& r) const { return x == r.x && y == r.y && z == r.z; }
    bool operator!=(const Vector3& r) const { return !(*this == r); }

    float LengthSquared() const { return x * x + y * y + z * z; }
    float Length() const;
};

/** @brief 点积。 */
inline float Dot(const Vector3& a, const Vector3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

} // namespace me
```

- [ ] **Step 4: 写 Vector3.cpp**

```cpp
#include "me/core/Vector3.h"

#include <cmath>

namespace me {

float Vector3::Length() const {
    return std::sqrt(LengthSquared());
}

} // namespace me
```
加 `src/Vector3.cpp` 到 `engine/core/CMakeLists.txt`。

- [ ] **Step 5: 写 Vector4.h(header-only,纯数据)**

```cpp
#pragma once

namespace me {

/** @brief 四维向量(主要用作 RGBA 颜色 / 齐次坐标)。 */
struct Vector4 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;

    constexpr Vector4() = default;
    constexpr Vector4(float xIn, float yIn, float zIn, float wIn)
        : x(xIn), y(yIn), z(zIn), w(wIn) {}

    Vector4 operator+(const Vector4& r) const { return {x + r.x, y + r.y, z + r.z, w + r.w}; }
    Vector4 operator-(const Vector4& r) const { return {x - r.x, y - r.y, z - r.z, w - r.w}; }
    Vector4 operator*(float s) const { return {x * s, y * s, z * s, w * s}; }
    bool operator==(const Vector4& r) const {
        return x == r.x && y == r.y && z == r.z && w == r.w;
    }
    bool operator!=(const Vector4& r) const { return !(*this == r); }
};

} // namespace me
```

- [ ] **Step 6: 跑测试确认通过**

Run: `cmake --build build -j && ctest --test-dir build --output-on-failure`
Expected: 全部 PASS。

- [ ] **Step 7: 提交**

```bash
git add engine/core tests
git commit -m "feat(core): Vector3 与 Vector4"
```

---

### Task 4: Core Matrix4x4(行主序 / 行向量)

**Files:**
- Create: `engine/core/include/me/core/Matrix4x4.h`
- Create: `engine/core/src/Matrix4x4.cpp`
- Modify: `engine/core/CMakeLists.txt`(加 `src/Matrix4x4.cpp`)
- Modify: `tests/CMakeLists.txt`(加 `core/test_matrix.cpp`)
- Test: `tests/core/test_matrix.cpp`

**Interfaces:**
- Consumes: `me::Vector2`(Task 2)。
- Produces:
  - `struct me::Matrix4x4 { float m[4][4]; }`,**行主序存储**,**行向量约定**(`v' = v * M`,平移在第 4 行)。
  - 静态工厂:`Identity()`、`Translation(const Vector2&)`、`Scale(const Vector2&)`、`Rotation(float radians)`、`Orthographic(float left,float right,float bottom,float top,float nearZ,float farZ)`。
  - 成员:`operator*(const Matrix4x4&)`、`TransformPoint(const Vector2&)`(w=1)、`TransformVector(const Vector2&)`(w=0,无平移)。

- [ ] **Step 1: 写失败测试**

`tests/core/test_matrix.cpp`:
```cpp
#include <doctest/doctest.h>

#include "me/core/Matrix4x4.h"
#include "me/core/Vector2.h"
#include "me/core/MathConstants.h"

using me::Matrix4x4;
using me::Vector2;

TEST_CASE("identity transforms point unchanged") {
    Matrix4x4 id = Matrix4x4::Identity();
    Vector2 p = id.TransformPoint(Vector2{3.0f, 7.0f});
    CHECK(p.x == doctest::Approx(3.0f));
    CHECK(p.y == doctest::Approx(7.0f));
}

TEST_CASE("translation moves points but not vectors") {
    Matrix4x4 t = Matrix4x4::Translation(Vector2{10.0f, 5.0f});
    Vector2 p = t.TransformPoint(Vector2{1.0f, 1.0f});
    CHECK(p.x == doctest::Approx(11.0f));
    CHECK(p.y == doctest::Approx(6.0f));
    Vector2 v = t.TransformVector(Vector2{1.0f, 1.0f}); // 方向不受平移影响
    CHECK(v.x == doctest::Approx(1.0f));
    CHECK(v.y == doctest::Approx(1.0f));
}

TEST_CASE("scale scales points") {
    Matrix4x4 s = Matrix4x4::Scale(Vector2{2.0f, 3.0f});
    Vector2 p = s.TransformPoint(Vector2{4.0f, 5.0f});
    CHECK(p.x == doctest::Approx(8.0f));
    CHECK(p.y == doctest::Approx(15.0f));
}

TEST_CASE("rotation by 90deg is CCW (Y up)") {
    Matrix4x4 r = Matrix4x4::Rotation(me::kPi * 0.5f);
    Vector2 p = r.TransformPoint(Vector2{1.0f, 0.0f}); // (1,0) -> (0,1)
    CHECK(p.x == doctest::Approx(0.0f).epsilon(0.0001));
    CHECK(p.y == doctest::Approx(1.0f).epsilon(0.0001));
}

TEST_CASE("multiply applies left-to-right for row vectors") {
    // 行向量: p' = p * (S * T) => 先缩放后平移
    Matrix4x4 st = Matrix4x4::Scale(Vector2{2.0f, 2.0f}) * Matrix4x4::Translation(Vector2{1.0f, 1.0f});
    Vector2 p = st.TransformPoint(Vector2{3.0f, 4.0f}); // *2 -> (6,8) -> +1 -> (7,9)
    CHECK(p.x == doctest::Approx(7.0f));
    CHECK(p.y == doctest::Approx(9.0f));
}

TEST_CASE("orthographic maps screen rect to NDC") {
    // left=0,right=800,bottom=0,top=600 => (0,0)->(-1,-1), (800,600)->(1,1)
    Matrix4x4 o = Matrix4x4::Orthographic(0.0f, 800.0f, 0.0f, 600.0f, 0.0f, 1.0f);
    Vector2 lo = o.TransformPoint(Vector2{0.0f, 0.0f});
    Vector2 hi = o.TransformPoint(Vector2{800.0f, 600.0f});
    CHECK(lo.x == doctest::Approx(-1.0f));
    CHECK(lo.y == doctest::Approx(-1.0f));
    CHECK(hi.x == doctest::Approx(1.0f));
    CHECK(hi.y == doctest::Approx(1.0f));
}
```
加 `core/test_matrix.cpp` 到 `tests/CMakeLists.txt`。

- [ ] **Step 2: 跑测试确认失败**

Run: `cmake --build build -j 2>&1 | head -30`
Expected: 找不到 `me/core/Matrix4x4.h`,编译失败。

- [ ] **Step 3: 写 Matrix4x4.h**

```cpp
#pragma once

namespace me {

struct Vector2;

/**
 * @brief 4x4 矩阵。
 *
 * 约定:**行主序存储**(m[row][col]),**行向量**乘法(v' = v * M),
 * 平移分量位于第 4 行(m[3][0..2])。与 DirectXMath/HLSL 同源,
 * 便于 M1 起接入 DX12。坐标系:世界空间 Y 轴向上。
 */
struct Matrix4x4 {
    float m[4][4];

    /** @brief 单位矩阵。 */
    static Matrix4x4 Identity();
    /** @brief 平移矩阵。 */
    static Matrix4x4 Translation(const Vector2& t);
    /** @brief 缩放矩阵。 */
    static Matrix4x4 Scale(const Vector2& s);
    /** @brief 绕 +Z 轴逆时针旋转(弧度;Y 轴向上)。 */
    static Matrix4x4 Rotation(float radians);
    /**
     * @brief 左手正交投影(z 映射到 [0,1],适配 DX)。
     * 将 [left,right]x[bottom,top] 映射到 NDC [-1,1]x[-1,1]。
     */
    static Matrix4x4 Orthographic(float left, float right, float bottom, float top,
                                  float nearZ, float farZ);

    /** @brief 矩阵乘法(this * rhs)。 */
    Matrix4x4 operator*(const Matrix4x4& rhs) const;

    /** @brief 变换点(隐含 w=1,受平移影响)。 */
    Vector2 TransformPoint(const Vector2& p) const;
    /** @brief 变换方向向量(隐含 w=0,不受平移影响)。 */
    Vector2 TransformVector(const Vector2& v) const;
};

} // namespace me
```

- [ ] **Step 4: 写 Matrix4x4.cpp**

```cpp
#include "me/core/Matrix4x4.h"
#include "me/core/Vector2.h"

#include <cmath>

namespace me {

Matrix4x4 Matrix4x4::Identity() {
    Matrix4x4 r{};
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            r.m[i][j] = (i == j) ? 1.0f : 0.0f;
        }
    }
    return r;
}

Matrix4x4 Matrix4x4::Translation(const Vector2& t) {
    Matrix4x4 r = Identity();
    r.m[3][0] = t.x; // 行向量约定:平移在第 4 行
    r.m[3][1] = t.y;
    return r;
}

Matrix4x4 Matrix4x4::Scale(const Vector2& s) {
    Matrix4x4 r = Identity();
    r.m[0][0] = s.x;
    r.m[1][1] = s.y;
    return r;
}

Matrix4x4 Matrix4x4::Rotation(float radians) {
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    Matrix4x4 r = Identity();
    // 行向量绕 +Z 逆时针:[1,0]*R = [cos, sin]
    r.m[0][0] = c;  r.m[0][1] = s;
    r.m[1][0] = -s; r.m[1][1] = c;
    return r;
}

Matrix4x4 Matrix4x4::Orthographic(float left, float right, float bottom, float top,
                                  float nearZ, float farZ) {
    Matrix4x4 r{}; // 全零起步
    const float rl = right - left;
    const float tb = top - bottom;
    const float fn = farZ - nearZ;

    r.m[0][0] = 2.0f / rl;
    r.m[1][1] = 2.0f / tb;
    r.m[2][2] = 1.0f / fn;
    r.m[3][0] = -(right + left) / rl;
    r.m[3][1] = -(top + bottom) / tb;
    r.m[3][2] = -nearZ / fn;
    r.m[3][3] = 1.0f;
    return r;
}

Matrix4x4 Matrix4x4::operator*(const Matrix4x4& rhs) const {
    Matrix4x4 r{};
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) {
                sum += m[i][k] * rhs.m[k][j];
            }
            r.m[i][j] = sum;
        }
    }
    return r;
}

Vector2 Matrix4x4::TransformPoint(const Vector2& p) const {
    // [x y 0 1] * M ,取 x/y 分量
    const float x = p.x * m[0][0] + p.y * m[1][0] + m[3][0];
    const float y = p.x * m[0][1] + p.y * m[1][1] + m[3][1];
    return Vector2{x, y};
}

Vector2 Matrix4x4::TransformVector(const Vector2& v) const {
    // [x y 0 0] * M ,无平移项
    const float x = v.x * m[0][0] + v.y * m[1][0];
    const float y = v.x * m[0][1] + v.y * m[1][1];
    return Vector2{x, y};
}

} // namespace me
```
加 `src/Matrix4x4.cpp` 到 `engine/core/CMakeLists.txt`。

- [ ] **Step 5: 跑测试确认通过**

Run: `cmake --build build -j && ctest --test-dir build --output-on-failure`
Expected: 全部 PASS。

- [ ] **Step 6: 提交**

```bash
git add engine/core tests
git commit -m "feat(core): Matrix4x4(行主序/行向量,含正交投影)"
```

---

### Task 5: Core Transform2D

纯值类型(position/rotation/scale → 局部矩阵)。父子层级与脏标记是 Scene 的职责(M4),M0 不做(YAGNI)。

**Files:**
- Create: `engine/core/include/me/core/Transform2D.h`
- Create: `engine/core/src/Transform2D.cpp`
- Modify: `engine/core/CMakeLists.txt`(加 `src/Transform2D.cpp`)
- Modify: `tests/CMakeLists.txt`(加 `core/test_transform2d.cpp`)
- Test: `tests/core/test_transform2d.cpp`

**Interfaces:**
- Consumes: `me::Vector2`(Task 2)、`me::Matrix4x4`(Task 4)。
- Produces:
  - `struct me::Transform2D { Vector2 position; float rotation; Vector2 scale; }`(默认 position=0, rotation=0, scale=(1,1))。
  - `Matrix4x4 me::Transform2D::ToMatrix() const`:局部→父空间矩阵 = `Scale * Rotation * Translation`(行向量:先缩放、再旋转、后平移)。

- [ ] **Step 1: 写失败测试**

`tests/core/test_transform2d.cpp`:
```cpp
#include <doctest/doctest.h>

#include "me/core/Transform2D.h"
#include "me/core/Vector2.h"
#include "me/core/MathConstants.h"

using me::Transform2D;
using me::Vector2;

TEST_CASE("default transform is identity-like") {
    Transform2D t;
    CHECK(t.position == Vector2{0.0f, 0.0f});
    CHECK(t.rotation == doctest::Approx(0.0f));
    CHECK(t.scale == Vector2{1.0f, 1.0f});
    Vector2 p = t.ToMatrix().TransformPoint(Vector2{2.0f, 3.0f});
    CHECK(p.x == doctest::Approx(2.0f));
    CHECK(p.y == doctest::Approx(3.0f));
}

TEST_CASE("transform applies scale then translation") {
    Transform2D t;
    t.position = Vector2{10.0f, 5.0f};
    t.scale = Vector2{2.0f, 3.0f};
    Vector2 p = t.ToMatrix().TransformPoint(Vector2{1.0f, 1.0f});
    // 缩放 (2,3) -> (2,3) 再平移 (10,5) -> (12,8)
    CHECK(p.x == doctest::Approx(12.0f));
    CHECK(p.y == doctest::Approx(8.0f));
}

TEST_CASE("transform applies rotation about origin before translation") {
    Transform2D t;
    t.position = Vector2{5.0f, 0.0f};
    t.rotation = me::kPi * 0.5f; // 90deg CCW
    Vector2 p = t.ToMatrix().TransformPoint(Vector2{1.0f, 0.0f});
    // (1,0) 旋转 90 -> (0,1) 再平移 (5,0) -> (5,1)
    CHECK(p.x == doctest::Approx(5.0f).epsilon(0.0001));
    CHECK(p.y == doctest::Approx(1.0f).epsilon(0.0001));
}
```
加 `core/test_transform2d.cpp` 到 `tests/CMakeLists.txt`。

- [ ] **Step 2: 跑测试确认失败**

Run: `cmake --build build -j 2>&1 | head -30`
Expected: 找不到 `me/core/Transform2D.h`,编译失败。

- [ ] **Step 3: 写 Transform2D.h**

```cpp
#pragma once

#include "me/core/Vector2.h"

namespace me {

struct Matrix4x4;

/**
 * @brief 2D 变换(纯值类型):位置 / 旋转(弧度)/ 缩放。
 *
 * 父子层级与脏标记由 Scene 层(M4)负责,本类型只描述单个局部变换。
 */
struct Transform2D {
    Vector2 position{0.0f, 0.0f};
    float   rotation = 0.0f;        ///< 弧度,绕 +Z 逆时针
    Vector2 scale{1.0f, 1.0f};

    /** @brief 生成局部→父空间矩阵(行向量:Scale * Rotation * Translation)。 */
    Matrix4x4 ToMatrix() const;
};

} // namespace me
```

- [ ] **Step 4: 写 Transform2D.cpp**

```cpp
#include "me/core/Transform2D.h"
#include "me/core/Matrix4x4.h"

namespace me {

Matrix4x4 Transform2D::ToMatrix() const {
    // 行向量约定:p' = p * S * R * T(先缩放、再旋转、后平移)
    return Matrix4x4::Scale(scale)
         * Matrix4x4::Rotation(rotation)
         * Matrix4x4::Translation(position);
}

} // namespace me
```
加 `src/Transform2D.cpp` 到 `engine/core/CMakeLists.txt`。

- [ ] **Step 5: 跑测试确认通过**

Run: `cmake --build build -j && ctest --test-dir build --output-on-failure`
Expected: 全部 PASS。

- [ ] **Step 6: 提交**

```bash
git add engine/core tests
git commit -m "feat(core): Transform2D(局部矩阵合成)"
```

---

### Task 6: Core Rect 与 AABB

**Files:**
- Create: `engine/core/include/me/core/Rect.h`
- Create: `engine/core/src/Rect.cpp`
- Create: `engine/core/include/me/core/AABB.h`
- Create: `engine/core/src/AABB.cpp`
- Modify: `engine/core/CMakeLists.txt`(加 `src/Rect.cpp`、`src/AABB.cpp`)
- Modify: `tests/CMakeLists.txt`(加 `core/test_rect_aabb.cpp`)
- Test: `tests/core/test_rect_aabb.cpp`

**Interfaces:**
- Consumes: `me::Vector2`(Task 2)。
- Produces:
  - `struct me::Rect { float x,y,width,height; }`:`Contains(const Vector2&)`、`Intersects(const Rect&)`、`Min()`/`Max()`(返回 `Vector2`)。约定 width/height ≥ 0。
  - `struct me::AABB { Vector2 min, max; }`:`Contains(const Vector2&)`、`Intersects(const AABB&)`、`Center()`、`Extents()`(半尺寸)、`Expanded(const Vector2&)`(并入点后的新 AABB)。

- [ ] **Step 1: 写失败测试**

`tests/core/test_rect_aabb.cpp`:
```cpp
#include <doctest/doctest.h>

#include "me/core/Rect.h"
#include "me/core/AABB.h"
#include "me/core/Vector2.h"

using me::Rect;
using me::AABB;
using me::Vector2;

TEST_CASE("rect contains point") {
    Rect r{0.0f, 0.0f, 10.0f, 4.0f};
    CHECK(r.Contains(Vector2{5.0f, 2.0f}));
    CHECK(r.Contains(Vector2{0.0f, 0.0f}));   // 左下边界含
    CHECK_FALSE(r.Contains(Vector2{10.0f, 2.0f})); // 右上边界不含(半开区间)
    CHECK_FALSE(r.Contains(Vector2{-1.0f, 2.0f}));
    CHECK(r.Min() == Vector2{0.0f, 0.0f});
    CHECK(r.Max() == Vector2{10.0f, 4.0f});
}

TEST_CASE("rect intersects") {
    Rect a{0.0f, 0.0f, 10.0f, 10.0f};
    Rect b{5.0f, 5.0f, 10.0f, 10.0f};
    Rect c{20.0f, 20.0f, 1.0f, 1.0f};
    CHECK(a.Intersects(b));
    CHECK_FALSE(a.Intersects(c));
}

TEST_CASE("aabb center extents and contains") {
    AABB box{Vector2{-2.0f, -4.0f}, Vector2{2.0f, 4.0f}};
    CHECK(box.Center() == Vector2{0.0f, 0.0f});
    CHECK(box.Extents() == Vector2{2.0f, 4.0f});
    CHECK(box.Contains(Vector2{0.0f, 0.0f}));
    CHECK_FALSE(box.Contains(Vector2{3.0f, 0.0f}));
}

TEST_CASE("aabb intersects and expand") {
    AABB a{Vector2{0.0f, 0.0f}, Vector2{4.0f, 4.0f}};
    AABB b{Vector2{2.0f, 2.0f}, Vector2{6.0f, 6.0f}};
    AABB far{Vector2{10.0f, 10.0f}, Vector2{12.0f, 12.0f}};
    CHECK(a.Intersects(b));
    CHECK_FALSE(a.Intersects(far));

    AABB e = a.Expanded(Vector2{-1.0f, 5.0f});
    CHECK(e.min == Vector2{-1.0f, 0.0f});
    CHECK(e.max == Vector2{4.0f, 5.0f});
}
```
加 `core/test_rect_aabb.cpp` 到 `tests/CMakeLists.txt`。

- [ ] **Step 2: 跑测试确认失败**

Run: `cmake --build build -j 2>&1 | head -30`
Expected: 找不到 `me/core/Rect.h` / `me/core/AABB.h`,编译失败。

- [ ] **Step 3: 写 Rect.h**

```cpp
#pragma once

#include "me/core/Vector2.h"

namespace me {

/** @brief 轴对齐矩形,左下角 (x,y) + 尺寸 (width,height)。约定 width/height >= 0。 */
struct Rect {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;

    /** @brief 左下角。 */
    Vector2 Min() const { return Vector2{x, y}; }
    /** @brief 右上角。 */
    Vector2 Max() const { return Vector2{x + width, y + height}; }

    /** @brief 点是否在矩形内(半开区间 [min, max))。 */
    bool Contains(const Vector2& p) const;
    /** @brief 与另一矩形是否相交(边接触视为不相交)。 */
    bool Intersects(const Rect& other) const;
};

} // namespace me
```

- [ ] **Step 4: 写 Rect.cpp**

```cpp
#include "me/core/Rect.h"

namespace me {

bool Rect::Contains(const Vector2& p) const {
    return p.x >= x && p.x < (x + width)
        && p.y >= y && p.y < (y + height);
}

bool Rect::Intersects(const Rect& other) const {
    return x < (other.x + other.width)
        && (x + width) > other.x
        && y < (other.y + other.height)
        && (y + height) > other.y;
}

} // namespace me
```

- [ ] **Step 5: 写 AABB.h**

```cpp
#pragma once

#include "me/core/Vector2.h"

namespace me {

/** @brief 轴对齐包围盒,由 min/max 两角定义。约定 min <= max(逐分量)。 */
struct AABB {
    Vector2 min{0.0f, 0.0f};
    Vector2 max{0.0f, 0.0f};

    /** @brief 中心点。 */
    Vector2 Center() const { return (min + max) * 0.5f; }
    /** @brief 半尺寸(extents)。 */
    Vector2 Extents() const { return (max - min) * 0.5f; }

    /** @brief 点是否在盒内(闭区间 [min, max])。 */
    bool Contains(const Vector2& p) const;
    /** @brief 与另一盒是否相交(含边接触)。 */
    bool Intersects(const AABB& other) const;
    /** @brief 返回把点 p 并入后的新包围盒。 */
    AABB Expanded(const Vector2& p) const;
};

} // namespace me
```

- [ ] **Step 6: 写 AABB.cpp**

```cpp
#include "me/core/AABB.h"

#include <algorithm>

namespace me {

bool AABB::Contains(const Vector2& p) const {
    return p.x >= min.x && p.x <= max.x
        && p.y >= min.y && p.y <= max.y;
}

bool AABB::Intersects(const AABB& other) const {
    return min.x <= other.max.x && max.x >= other.min.x
        && min.y <= other.max.y && max.y >= other.min.y;
}

AABB AABB::Expanded(const Vector2& p) const {
    AABB r;
    r.min = Vector2{std::min(min.x, p.x), std::min(min.y, p.y)};
    r.max = Vector2{std::max(max.x, p.x), std::max(max.y, p.y)};
    return r;
}

} // namespace me
```
加 `src/Rect.cpp`、`src/AABB.cpp` 到 `engine/core/CMakeLists.txt`。

- [ ] **Step 7: 跑测试确认通过**

Run: `cmake --build build -j && ctest --test-dir build --output-on-failure`
Expected: 全部 PASS。

- [ ] **Step 8: 提交**

```bash
git add engine/core tests
git commit -m "feat(core): Rect 与 AABB"
```

---

### Task 7: Core Handle<T>(类型化句柄)

`index + generation` 句柄,用于后续 AssetManager/SparseSet 的类型安全引用。

**Files:**
- Create: `engine/core/include/me/core/Handle.h`
- Modify: `tests/CMakeLists.txt`(加 `core/test_handle.cpp`)
- Test: `tests/core/test_handle.cpp`

**Interfaces:**
- Consumes: 无。
- Produces:
  - `template<typename T> struct me::Handle { uint32_t index; uint32_t generation; };`
  - `bool IsValid() const`、`operator==`、`operator!=`;静态 `Invalid()`;约定无效句柄 `index == kInvalidIndex`(具名常量)。

- [ ] **Step 1: 写失败测试**

`tests/core/test_handle.cpp`:
```cpp
#include <doctest/doctest.h>

#include "me/core/Handle.h"

struct Texture; // 仅用作类型标签

using me::Handle;

TEST_CASE("default handle is invalid") {
    Handle<Texture> h;
    CHECK_FALSE(h.IsValid());
    CHECK(h == Handle<Texture>::Invalid());
}

TEST_CASE("constructed handle is valid and comparable") {
    Handle<Texture> a{3u, 1u};
    Handle<Texture> b{3u, 1u};
    Handle<Texture> c{3u, 2u}; // 同 index 不同 generation
    CHECK(a.IsValid());
    CHECK(a == b);
    CHECK(a != c);
}
```
加 `core/test_handle.cpp` 到 `tests/CMakeLists.txt`。

- [ ] **Step 2: 跑测试确认失败**

Run: `cmake --build build -j 2>&1 | head -30`
Expected: 找不到 `me/core/Handle.h`,编译失败。

- [ ] **Step 3: 写 Handle.h(header-only 模板)**

```cpp
#pragma once

#include <cstdint>

namespace me {

/// 句柄无效索引哨兵(零魔法数字:具名常量)。
constexpr uint32_t kInvalidHandleIndex = 0xFFFFFFFFu;

/**
 * @brief 类型化资源句柄(index + generation)。
 *
 * 模板参数 T 仅作类型标签,使不同资源句柄不可互相赋值,获得编译期类型安全。
 * generation 用于检测“悬垂句柄”(槽位被复用后旧句柄失效)。
 */
template <typename T>
struct Handle {
    uint32_t index = kInvalidHandleIndex;
    uint32_t generation = 0u;

    /** @brief 是否为有效句柄。 */
    bool IsValid() const { return index != kInvalidHandleIndex; }

    /** @brief 无效句柄常量。 */
    static Handle Invalid() { return Handle{}; }

    bool operator==(const Handle& r) const {
        return index == r.index && generation == r.generation;
    }
    bool operator!=(const Handle& r) const { return !(*this == r); }
};

} // namespace me
```

- [ ] **Step 4: 跑测试确认通过**

Run: `cmake --build build -j && ctest --test-dir build --output-on-failure`
Expected: 全部 PASS。

- [ ] **Step 5: 提交**

```bash
git add engine/core tests
git commit -m "feat(core): Handle<T> 类型化句柄"
```

---

### Task 8: Core Log 与 Assert

日志拆出**纯函数格式化器**(可单测)+ 写 stderr 的宏(无全局可变状态,符合架构禁令)。断言提供 `ME_ASSERT`,Release 编译为空操作。

**Files:**
- Create: `engine/core/include/me/core/Log.h`
- Create: `engine/core/src/Log.cpp`
- Create: `engine/core/include/me/core/Assert.h`
- Create: `engine/core/src/Assert.cpp`
- Modify: `engine/core/CMakeLists.txt`(加 `src/Log.cpp`、`src/Assert.cpp`)
- Modify: `tests/CMakeLists.txt`(加 `core/test_log.cpp`)
- Test: `tests/core/test_log.cpp`

**Interfaces:**
- Consumes: 无。
- Produces:
  - `enum class me::LogLevel { Trace, Info, Warning, Error };`
  - `std::string me::FormatLogLine(LogLevel, const std::string& msg);` → 形如 `"[INFO] msg"`(纯函数,可测)。
  - `void me::LogWrite(LogLevel, const std::string& msg);`(写 stderr)。
  - 宏:`ME_LOG_TRACE/INFO/WARN/ERROR(msgString)`。
  - `me/core/Assert.h`:宏 `ME_ASSERT(expr)`、`ME_ASSERT_MSG(expr, msgCstr)`;失败钩子 `void me::detail::AssertFail(const char* expr, const char* file, int line, const char* msg);`(写 stderr 后 `std::abort()`)。Release(`NDEBUG`)下宏为 `((void)0)`。

- [ ] **Step 1: 写失败测试**

`tests/core/test_log.cpp`:
```cpp
#include <doctest/doctest.h>

#include "me/core/Log.h"
#include "me/core/Assert.h"

using me::LogLevel;

TEST_CASE("format log line prepends level tag") {
    CHECK(me::FormatLogLine(LogLevel::Info, "hello") == "[INFO] hello");
    CHECK(me::FormatLogLine(LogLevel::Warning, "careful") == "[WARN] careful");
    CHECK(me::FormatLogLine(LogLevel::Error, "boom") == "[ERROR] boom");
    CHECK(me::FormatLogLine(LogLevel::Trace, "step") == "[TRACE] step");
}

TEST_CASE("log macros do not crash") {
    ME_LOG_INFO(std::string("info via macro"));
    ME_LOG_WARN(std::string("warn via macro"));
}

TEST_CASE("passing assert is a no-op") {
    ME_ASSERT(1 + 1 == 2);
    ME_ASSERT_MSG(true, "should hold");
    CHECK(true);
}
```
加 `core/test_log.cpp` 到 `tests/CMakeLists.txt`。

- [ ] **Step 2: 跑测试确认失败**

Run: `cmake --build build -j 2>&1 | head -30`
Expected: 找不到 `me/core/Log.h` / `me/core/Assert.h`,编译失败。

- [ ] **Step 3: 写 Log.h**

```cpp
#pragma once

#include <string>

namespace me {

/** @brief 日志级别。 */
enum class LogLevel { Trace, Info, Warning, Error };

/** @brief 把级别+消息格式化为单行字符串(纯函数,便于单测)。 */
std::string FormatLogLine(LogLevel level, const std::string& msg);

/** @brief 将一行日志写到标准错误输出。 */
void LogWrite(LogLevel level, const std::string& msg);

} // namespace me

// 便捷宏:参数为 std::string(M0 不做 printf 风格变参,保持简单/安全)。
#define ME_LOG_TRACE(msg) ::me::LogWrite(::me::LogLevel::Trace,   (msg))
#define ME_LOG_INFO(msg)  ::me::LogWrite(::me::LogLevel::Info,    (msg))
#define ME_LOG_WARN(msg)  ::me::LogWrite(::me::LogLevel::Warning, (msg))
#define ME_LOG_ERROR(msg) ::me::LogWrite(::me::LogLevel::Error,   (msg))
```

- [ ] **Step 4: 写 Log.cpp**

```cpp
#include "me/core/Log.h"

#include <cstdio>

namespace me {

namespace {
const char* LevelTag(LogLevel level) {
    switch (level) {
        case LogLevel::Trace:   return "TRACE";
        case LogLevel::Info:    return "INFO";
        case LogLevel::Warning: return "WARN";
        case LogLevel::Error:   return "ERROR";
    }
    return "INFO"; // 不可达:全部枚举已覆盖
}
} // namespace

std::string FormatLogLine(LogLevel level, const std::string& msg) {
    std::string out;
    out.reserve(msg.size() + 8);
    out += '[';
    out += LevelTag(level);
    out += "] ";
    out += msg;
    return out;
}

void LogWrite(LogLevel level, const std::string& msg) {
    const std::string line = FormatLogLine(level, msg);
    std::fprintf(stderr, "%s\n", line.c_str());
}

} // namespace me
```

- [ ] **Step 5: 写 Assert.h**

```cpp
#pragma once

namespace me::detail {

/** @brief 断言失败处理:写诊断到 stderr 后 std::abort()。 */
[[noreturn]] void AssertFail(const char* expr, const char* file, int line, const char* msg);

} // namespace me::detail

#if defined(NDEBUG)
    // Release:断言编译为空操作。
    #define ME_ASSERT(expr)          ((void)0)
    #define ME_ASSERT_MSG(expr, msg) ((void)0)
#else
    /** @brief 调试断言:expr 为假则打印并中止。 */
    #define ME_ASSERT(expr) \
        ((expr) ? (void)0 : ::me::detail::AssertFail(#expr, __FILE__, __LINE__, nullptr))
    /** @brief 带消息的调试断言。 */
    #define ME_ASSERT_MSG(expr, msg) \
        ((expr) ? (void)0 : ::me::detail::AssertFail(#expr, __FILE__, __LINE__, (msg)))
#endif
```

- [ ] **Step 6: 写 Assert.cpp**

```cpp
#include "me/core/Assert.h"

#include <cstdio>
#include <cstdlib>

namespace me::detail {

void AssertFail(const char* expr, const char* file, int line, const char* msg) {
    if (msg != nullptr) {
        std::fprintf(stderr, "[ASSERT] %s:%d: (%s) -- %s\n", file, line, expr, msg);
    } else {
        std::fprintf(stderr, "[ASSERT] %s:%d: (%s)\n", file, line, expr);
    }
    std::abort();
}

} // namespace me::detail
```
加 `src/Log.cpp`、`src/Assert.cpp` 到 `engine/core/CMakeLists.txt`。

- [ ] **Step 7: 跑测试确认通过**

Run: `cmake --build build -j && ctest --test-dir build --output-on-failure`
Expected: 全部 PASS。

- [ ] **Step 8: 提交**

```bash
git add engine/core tests
git commit -m "feat(core): Log(可测格式化器)与 Assert"
```

---

### Task 9: Platform Time(高精度计时)

跨平台 `Stopwatch`(基于 `std::chrono::steady_clock`)+ `FrameTimer`(逐帧 deltaTime)。

**Files:**
- Create: `engine/platform/include/me/platform/Time.h`
- Create: `engine/platform/src/Time.cpp`
- Modify: `engine/platform/CMakeLists.txt`(加 `src/Time.cpp`)
- Modify: `tests/CMakeLists.txt`(加 `platform/test_time.cpp`)
- Test: `tests/platform/test_time.cpp`

**Interfaces:**
- Consumes: 无(仅标准库)。
- Produces:
  - `class me::platform::Stopwatch`:构造即开始;`void Restart()`、`double ElapsedSeconds() const`、`double ElapsedMilliseconds() const`。
  - `class me::platform::FrameTimer`:`double Tick()` 返回距上次 `Tick()` 的秒数(首帧返回 0),`double TotalSeconds() const`。

- [ ] **Step 1: 写失败测试**

`tests/platform/test_time.cpp`:
```cpp
#include <doctest/doctest.h>

#include "me/platform/Time.h"

#include <thread>
#include <chrono>

using me::platform::Stopwatch;
using me::platform::FrameTimer;

TEST_CASE("stopwatch elapsed is non-negative and monotonic") {
    Stopwatch sw;
    const double t1 = sw.ElapsedSeconds();
    const double t2 = sw.ElapsedSeconds();
    CHECK(t1 >= 0.0);
    CHECK(t2 >= t1);
    CHECK(sw.ElapsedMilliseconds() >= 0.0);
}

TEST_CASE("stopwatch measures a real delay") {
    Stopwatch sw;
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    CHECK(sw.ElapsedSeconds() >= 0.010); // 宽松下界,避开调度抖动
}

TEST_CASE("frame timer first tick is zero, total accumulates") {
    FrameTimer ft;
    CHECK(ft.Tick() == doctest::Approx(0.0)); // 首帧无历史
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    const double dt = ft.Tick();
    CHECK(dt >= 0.0);
    CHECK(ft.TotalSeconds() >= dt);
}
```
加 `platform/test_time.cpp` 到 `tests/CMakeLists.txt`。

- [ ] **Step 2: 跑测试确认失败**

Run: `cmake --build build -j 2>&1 | head -30`
Expected: 找不到 `me/platform/Time.h`,编译失败。

- [ ] **Step 3: 写 Time.h**

```cpp
#pragma once

#include <chrono>

namespace me::platform {

/** @brief 单调高精度秒表(基于 steady_clock)。 */
class Stopwatch {
public:
    /** @brief 构造即开始计时。 */
    Stopwatch() : m_start(Clock::now()) {}

    /** @brief 重置起点为当前时刻。 */
    void Restart() { m_start = Clock::now(); }

    /** @brief 自起点经过的秒数。 */
    double ElapsedSeconds() const;
    /** @brief 自起点经过的毫秒数。 */
    double ElapsedMilliseconds() const;

private:
    using Clock = std::chrono::steady_clock;
    Clock::time_point m_start;
};

/** @brief 逐帧计时器:每次 Tick 返回距上次 Tick 的秒数。 */
class FrameTimer {
public:
    FrameTimer();

    /** @brief 推进一帧,返回 deltaTime(秒);首次调用返回 0。 */
    double Tick();
    /** @brief 自创建以来累计的总秒数。 */
    double TotalSeconds() const { return m_total; }

private:
    using Clock = std::chrono::steady_clock;
    Clock::time_point m_last;
    double m_total = 0.0;
    bool   m_firstTick = true;
};

} // namespace me::platform
```

- [ ] **Step 4: 写 Time.cpp**

```cpp
#include "me/platform/Time.h"

namespace me::platform {

double Stopwatch::ElapsedSeconds() const {
    const std::chrono::duration<double> d = Clock::now() - m_start;
    return d.count();
}

double Stopwatch::ElapsedMilliseconds() const {
    const std::chrono::duration<double, std::milli> d = Clock::now() - m_start;
    return d.count();
}

FrameTimer::FrameTimer() : m_last(Clock::now()) {}

double FrameTimer::Tick() {
    const Clock::time_point now = Clock::now();
    if (m_firstTick) {
        m_firstTick = false;
        m_last = now;
        return 0.0; // 首帧无历史,delta 视为 0
    }
    const std::chrono::duration<double> d = now - m_last;
    m_last = now;
    const double dt = d.count();
    m_total += dt;
    return dt;
}

} // namespace me::platform
```
加 `src/Time.cpp` 到 `engine/platform/CMakeLists.txt`。

- [ ] **Step 5: 跑测试确认通过**

Run: `cmake --build build -j && ctest --test-dir build --output-on-failure`
Expected: 全部 PASS。

- [ ] **Step 6: 提交**

```bash
git add engine/platform tests
git commit -m "feat(platform): Time(Stopwatch + FrameTimer)"
```

---

### Task 10: Platform FileSystem(无异常文件 IO)

跨平台文件读写,失败返回 `std::optional`/`bool`,绝不抛异常。

**Files:**
- Create: `engine/platform/include/me/platform/FileSystem.h`
- Create: `engine/platform/src/FileSystem.cpp`
- Modify: `engine/platform/CMakeLists.txt`(加 `src/FileSystem.cpp`)
- Modify: `tests/CMakeLists.txt`(加 `platform/test_filesystem.cpp`)
- Test: `tests/platform/test_filesystem.cpp`

**Interfaces:**
- Consumes: 无(仅标准库 `<fstream>`/`<filesystem>`)。
- Produces(命名空间 `me::platform`):
  - `bool Exists(const std::string& path);`
  - `std::optional<std::string> ReadTextFile(const std::string& path);`
  - `std::optional<std::vector<std::uint8_t>> ReadBinaryFile(const std::string& path);`
  - `bool WriteTextFile(const std::string& path, const std::string& content);`

- [ ] **Step 1: 写失败测试**

`tests/platform/test_filesystem.cpp`:
```cpp
#include <doctest/doctest.h>

#include "me/platform/FileSystem.h"

#include <filesystem>
#include <string>

namespace fs = std::filesystem;
namespace pf = me::platform;

TEST_CASE("write then read text roundtrips") {
    const fs::path tmp = fs::temp_directory_path() / "me_fs_test.txt";
    const std::string content = "line1\nline2\n";

    CHECK(pf::WriteTextFile(tmp.string(), content));
    CHECK(pf::Exists(tmp.string()));

    auto read = pf::ReadTextFile(tmp.string());
    REQUIRE(read.has_value());
    CHECK(*read == content);

    auto bin = pf::ReadBinaryFile(tmp.string());
    REQUIRE(bin.has_value());
    CHECK(bin->size() == content.size());

    fs::remove(tmp);
}

TEST_CASE("missing file reports absent and returns nullopt") {
    const std::string missing = (fs::temp_directory_path() / "me_fs_does_not_exist_42.txt").string();
    CHECK_FALSE(pf::Exists(missing));
    CHECK_FALSE(pf::ReadTextFile(missing).has_value());
    CHECK_FALSE(pf::ReadBinaryFile(missing).has_value());
}
```
加 `platform/test_filesystem.cpp` 到 `tests/CMakeLists.txt`。

- [ ] **Step 2: 跑测试确认失败**

Run: `cmake --build build -j 2>&1 | head -30`
Expected: 找不到 `me/platform/FileSystem.h`,编译失败。

- [ ] **Step 3: 写 FileSystem.h**

```cpp
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace me::platform {

/** @brief 路径是否存在且为常规文件。 */
bool Exists(const std::string& path);

/** @brief 读取整个文本文件;失败返回 std::nullopt(不抛异常)。 */
std::optional<std::string> ReadTextFile(const std::string& path);

/** @brief 读取整个二进制文件;失败返回 std::nullopt(不抛异常)。 */
std::optional<std::vector<std::uint8_t>> ReadBinaryFile(const std::string& path);

/** @brief 写文本文件(覆盖);成功返回 true。 */
bool WriteTextFile(const std::string& path, const std::string& content);

} // namespace me::platform
```

- [ ] **Step 4: 写 FileSystem.cpp**

```cpp
#include "me/platform/FileSystem.h"

#include <filesystem>
#include <fstream>
#include <system_error>

namespace me::platform {

bool Exists(const std::string& path) {
    std::error_code ec; // 无异常重载
    return std::filesystem::is_regular_file(path, ec) && !ec;
}

std::optional<std::string> ReadTextFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return std::nullopt;
    }
    std::string content((std::istreambuf_iterator<char>(in)),
                        std::istreambuf_iterator<char>());
    if (in.bad()) {
        return std::nullopt;
    }
    return content;
}

std::optional<std::vector<std::uint8_t>> ReadBinaryFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return std::nullopt;
    }
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                                    std::istreambuf_iterator<char>());
    if (in.bad()) {
        return std::nullopt;
    }
    return bytes;
}

bool WriteTextFile(const std::string& path, const std::string& content) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        return false;
    }
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    return static_cast<bool>(out);
}

} // namespace me::platform
```
加 `src/FileSystem.cpp` 到 `engine/platform/CMakeLists.txt`。

> 注:某些较老的 GCC/Clang 链接 `std::filesystem` 需 `-lstdc++fs`/`-lc++fs`。若链接报 `filesystem` 未定义符号,在 `engine/platform/CMakeLists.txt` 加:`target_link_libraries(me_platform PUBLIC stdc++fs)`(仅老编译器需要;GCC ≥ 9 / Clang ≥ 9 通常不需要)。

- [ ] **Step 5: 跑测试确认通过**

Run: `cmake --build build -j && ctest --test-dir build --output-on-failure`
Expected: 全部 PASS。

- [ ] **Step 6: 提交**

```bash
git add engine/platform tests
git commit -m "feat(platform): FileSystem(无异常文件读写)"
```

---

### Task 11: 模块文档 + PROGRESS 回写

收尾:补模块说明文档(CLAUDE.md 要求新增模块同步文档),回写跨会话进度。

**Files:**
- Create: `engine/core/README.md`
- Create: `engine/platform/README.md`
- Create: `README.md`(根:构建说明;当前为空文件,改写)
- Modify: `docs/PROGRESS.md`

**Interfaces:**
- Consumes: 前述全部产物。
- Produces: 文档,无代码接口。

- [ ] **Step 1: 写 engine/core/README.md**

```markdown
# me_core

零外部依赖的基础设施层。namespace `me`。

## 内容(M0)
- **数学(2D)**:`Vector2/3/4`、`Matrix4x4`(行主序/行向量,含正交投影)、`Transform2D`、`Rect`、`AABB`、`MathConstants`(具名常量)。
- **句柄**:`Handle<T>`(index + generation,类型安全)。
- **日志/断言**:`Log`(`FormatLogLine` 纯函数 + `ME_LOG_*` 宏)、`Assert`(`ME_ASSERT` / `ME_ASSERT_MSG`,Release 空操作)。

## 约定
- 坐标系:世界空间 **Y 轴向上**,正交投影。
- 矩阵:**行主序存储 + 行向量**(`p' = p * M`,平移在第 4 行),与 DX12/DirectXMath 同源。
- 无异常;不变量违反用 `ME_ASSERT`。

## 依赖
无(下层,任何模块不得被它反向依赖)。
```

- [ ] **Step 2: 写 engine/platform/README.md**

```markdown
# me_platform

操作系统隔离层。namespace `me::platform`。依赖 `me_core`。

## 内容(M0,跨平台)
- **Time**:`Stopwatch`(单调秒表)、`FrameTimer`(逐帧 deltaTime)。
- **FileSystem**:`Exists` / `ReadTextFile` / `ReadBinaryFile` / `WriteTextFile`,失败返回 `optional`/`bool`,不抛异常。

## 推迟到 M1
- `Window` / `Input`(Win32):需真实 Windows 窗口验证,与 DX12 上屏一起做。

## 依赖
`me_core`(单向)。
```

- [ ] **Step 3: 改写根 README.md**

```markdown
# MiniEngine

面向星露谷物语类 **2D/2.5D 农场模拟游戏**的轻量级 C++ 引擎(学习向,Agent-ready Tool API)。
设计文档见 `docs/superpowers/specs/`,进度见 `docs/PROGRESS.md`。

## 构建(M0)

需要 CMake ≥ 3.20 与 C++17 编译器。首次配置会用 git 拉取 doctest。

```bash
cmake -S . -B build -DME_BUILD_TESTS=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## 模块(M0)
- `engine/core`(`me_core`):2D 数学、句柄、日志、断言。
- `engine/platform`(`me_platform`):计时、文件系统。
- `tests`(`me_tests`):doctest 单元测试。
```

- [ ] **Step 4: 回写 docs/PROGRESS.md**

把 M0 行状态从 `☐` 改为 `☑`,更新顶部"当前阶段""一句话现状",并在决策记录追加一行。具体编辑:

1. 顶部:
   - `**当前阶段**:M0 地基完成(Core + Platform 计时/文件系统 + 单测全绿),下一步 M1 精灵上屏`
2. 一句话现状:替换为
   - `M0 地基已完成:CMake 模块化骨架、Core(2D 数学/Handle/Log/Assert)、Platform(Time/FileSystem)均跨平台并通过 doctest 单测。下一步为 M1 写实现计划(含 Win32 Window/Input + DX12 最小上屏)。`
3. 里程碑表:`| **M0 地基** | ☑ | ... |`
4. 关键决策记录追加:
   - `| 2026-06-17 | M0 Core/Platform 跨平台,WSL 可单测;Win32 Window/Input 推迟到 M1 | Core/计时/文件系统不依赖窗口,可即时 TDD;窗口需真实 Windows 验证,与 DX12 上屏合并 |`
   - `| 2026-06-17 | 数学库定为行主序 + 行向量(p' = p*M) | 与 DX12/DirectXMath 同源,降低 M1 接图形 API 的心智负担 |`
   - `| 2026-06-17 | 单测框架选 doctest | 单头、编译快、API 简洁,契合最小依赖 |`

- [ ] **Step 5: 全量构建 + 测试,最终验证**

Run:
```bash
rm -rf build
cmake -S . -B build -DME_BUILD_TESTS=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
```
Expected: 干净配置+全量编译无错;`ctest` 报告所有测试 100% 通过。

- [ ] **Step 6: 提交**

```bash
git add README.md engine/core/README.md engine/platform/README.md docs/PROGRESS.md
git commit -m "docs(m0): 模块说明 + 构建说明 + 进度回写"
```

---

## Self-Review

**1. Spec 覆盖(对照 spec §5 Core / Platform 与 §13 M0 行):**
- CMake 模块化骨架 + 单向依赖 → Task 1。
- Core 2D 数学(Vector2/3/4、Matrix4x4、Transform2D、Rect、AABB)→ Task 2–6。
- Core Handle → Task 7;Log/Assert → Task 8。
- Platform Time/FileSystem → Task 9–10。Win32 Window/Input → 经用户确认推迟 M1(已在 PROGRESS 决策记录)。
- 单测(doctest)→ 每个 Task 内置。
- 文档同步(CLAUDE.md 要求)→ Task 11。
- spec Core 提到的 `Containers/SparseSet/线性分配器`、`Event`、`TypeId`:M0 未用到,按 YAGNI 推迟(M4 Scene / 资源池时引入)。这是有意决策,非遗漏。

**2. Placeholder 扫描:** 无 TBD/TODO/"自行处理";每个代码步骤含完整代码;每个测试步骤含完整断言与期望输出。

**3. 类型一致性:** 跨 Task 引用核对——`Vector2`/`Matrix4x4` 签名在 Task 2/4 定义,Task 5(Transform2D)、Task 6(Rect/AABB)按定义消费;`FormatLogLine`/`LogWrite`/`ME_LOG_*`/`ME_ASSERT` 在 Task 8 内自洽;`Stopwatch`/`FrameTimer`/`FileSystem` 函数签名在 Task 9/10 的 Interfaces 与代码一致。命名空间:Core=`me`,Platform=`me::platform`,全程一致。

> 已知非阻塞项:`std::filesystem` 老编译器链接(Task 10 Step 4 注)、doctest 首次需联网拉取(Task 1 Step 11)。两者均已在对应步骤标注。
