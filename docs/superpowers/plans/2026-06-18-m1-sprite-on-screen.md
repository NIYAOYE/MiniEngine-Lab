# M1 精灵上屏 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 用 DX12 + Win32 打开一个窗口、清屏、并画出一个带纹理的精灵,沿途把 RHI 同步/命令录制做成可验证的最小封装。

**Architecture:** 新增 4 个分层模块 —— `me_rhi_cpu`(跨平台、纯 CPU 逻辑,header-only)、`me_rhi`(Windows-only DX12 薄封装)、`me_assets`(stb_image 解码,跨平台)、`me_renderer`(单精灵绘制)。Win32 `Window`/`Input` 补进 `me_platform`。`sandbox` 可执行文件把它们串起来做目视验证。GPU 正确性由独立的 WARP 软件适配器 + 像素回读测试(`me_gpu_tests`)在 Windows 上把关。

**Tech Stack:** C++17 · DirectX 12(系统 SDK:`d3d12.lib`/`dxgi.lib`/`d3dcompiler.lib`)· FXC 运行时编译(`D3DCompileFromFile`)· stb_image · doctest · CMake ≥ 3.20(Visual Studio 17 2022 生成器)。

## Global Constraints

逐条来自项目 CLAUDE.md 与设计文档,每个任务都隐含遵守:

- **C++ 标准**:C++17,`CMAKE_CXX_EXTENSIONS OFF`,各 target `target_compile_features(... PUBLIC cxx_std_17)`。
- **MSVC 必须 `/utf-8`(已验证,关键)**:本项目源文件含中文注释,MSVC 默认按系统码页(GBK/936)读源码,某些中文字尾字节为 `\`(0x5C)会吃掉下一行导致编译错误(C4819 + C2065)。根 `CMakeLists.txt` 必须加 `add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/utf-8>)`。已用 `cl /utf-8` 实测编译/链接/运行 DX12 程序通过。
- **不使用 C++ 异常**:可恢复错误用返回值 / `std::optional` / 工厂返回 `nullptr`;不变量违反用 `ME_ASSERT`;DX12 `HRESULT` 用 `ME_HR_CHECK`(本计划 Task 4 新增)。
- **零魔法数字**:所有数值常量用具名 `constexpr`(如 `kFrameCount`、`kSpriteIndexCount`),不得出现裸数字判断/尺寸。
- **无硬编码内容**:贴图路径、着色器路径来自配置常量 / `ME_ASSET_DIR` 编译宏,不在逻辑里写死内容字符串。
- **Doxygen 注释**:每个公开 API 函数写 `/** */` 注释;非显然实现写行内注释。
- **命名**:类型/函数 `PascalCase`,成员 `m_camelCase`,常量 `kPascalCase`,命名空间 `me::rhi` / `me::assets` / `me::render`;头文件 `#pragma once`;头文件中禁止 `using namespace std`。
- **裸 `ID3D12*` 封死在 `me_rhi` 内**:不出现在 `me_renderer` 公开头文件以外?——`me_renderer` 的 `Draw` 形参允许接收 `ID3D12GraphicsCommandList*`(M1 折中,M2 引入 Renderer 帧封装后移除);除此之外不外泄。
- **单向依赖(CMake 强制)**:`me_rhi_cpu` ← 仅 Core;`me_rhi` → Core, Platform, me_rhi_cpu;`me_assets` → Core, Platform;`me_renderer` → Core, me_rhi, me_assets;`sandbox` → 全部。下层绝不反向依赖上层。
- **新增模块必须同步更新** `CMakeLists.txt` **与模块说明文档**(每个模块 `README.md`)。
- **数学约定(M0 已钉死)**:`Matrix4x4` 行主序存储 + 行向量乘法(`v' = v * M`),平移在第 4 行;世界空间 Y 轴向上;`Orthographic` 为左手、z∈[0,1]。HLSL 端必须用 `row_major float4x4` 接收以匹配。

## 构建与验证环境(重要)

DX12/Win32 代码**无法在 WSL 编译或运行**。本计划区分两套构建:

- **跨平台逻辑(可在 WSL 跑)**:`me_core` / `me_platform`(CPU 部分)/ `me_rhi_cpu` / `me_assets` / `me_tests`。
  ```bash
  cmake -S . -B build-wsl -DME_BUILD_TESTS=ON -DME_BUILD_GPU_TESTS=OFF
  cmake --build build-wsl -j
  ctest --test-dir build-wsl --output-on-failure
  ```
- **DX12/GPU + 目视(必须在 Windows 跑;Developer PowerShell for VS 2022)**:额外构建 `me_rhi` / `me_renderer` / `me_gpu_tests` / `sandbox`。
  ```powershell
  cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DME_BUILD_TESTS=ON -DME_BUILD_GPU_TESTS=ON
  cmake --build build --config Debug
  ctest --test-dir build -C Debug --output-on-failure
  .\build\bin\Debug\sandbox.exe
  ```

每个任务的"验证"步骤标注 **[WSL]**(可在 WSL 红绿)或 **[Windows]**(需真机)。

## 文件结构(本里程碑创建/修改)

```
engine/platform/
  include/me/platform/Window.h          ← 新建 (Task 1)
  include/me/platform/Input.h           ← 新建 (Task 2)
  src/Window_Win32.cpp                  ← 新建 (Task 1, if(WIN32))
  src/Input.cpp                         ← 新建 (Task 2, 跨平台状态机)
  src/Input_Win32.cpp                   ← 新建 (Task 2, if(WIN32) 消息→键映射)
  CMakeLists.txt                        ← 修改
engine/rhi/                             ← 新建模块
  include/me/rhi/FenceTracker.h         ← Task 3 (header-only, 跨平台)
  include/me/rhi/FrameRing.h            ← Task 3
  include/me/rhi/DescriptorAllocatorLogic.h ← Task 3
  include/me/rhi/QuadGeometry.h         ← Task 3
  include/me/rhi/SpriteTransform.h      ← Task 3
  include/me/rhi/D3DCommon.h            ← Task 4 (ME_HR_CHECK + ComPtr 别名, if(WIN32))
  include/me/rhi/GpuDevice.h            ← Task 5
  include/me/rhi/Fence.h                ← Task 5
  include/me/rhi/CommandContext.h       ← Task 6
  include/me/rhi/DescriptorHeap.h       ← Task 6
  include/me/rhi/GpuBuffer.h            ← Task 7
  include/me/rhi/GpuTexture.h           ← Task 8
  include/me/rhi/Readback.h             ← Task 8 (测试用回读)
  include/me/rhi/Shader.h               ← Task 9
  include/me/rhi/SwapChain.h            ← Task 11
  src/*.cpp                             ← 各任务对应
  CMakeLists.txt                        ← 新建
  README.md                             ← 新建
engine/assets/                          ← 新建模块
  include/me/assets/ImageData.h         ← Task 10
  src/ImageLoader.cpp                   ← Task 10
  src/stb_image_impl.cpp                ← Task 10
  CMakeLists.txt / README.md            ← 新建
engine/renderer/                        ← 新建模块
  include/me/render/SpriteRenderer.h    ← Task 12
  src/SpriteRenderer.cpp                ← Task 12
  CMakeLists.txt / README.md            ← 新建
assets/shaders/sprite.hlsl              ← Task 9
assets/textures/test_sprite.png        ← Task 13 (放入一张测试图)
sandbox/
  main.cpp                              ← Task 13
  CMakeLists.txt                        ← 新建
third_party/CMakeLists.txt              ← 修改 (stb)
tests/CMakeLists.txt                    ← 修改
tests/rhi/test_rhi_logic.cpp           ← Task 3
tests/assets/test_image.cpp            ← Task 10
tests/gpu/gpu_test_main.cpp            ← Task 5 (Windows-only target)
tests/gpu/test_device_fence.cpp        ← Task 5
tests/gpu/test_texture_roundtrip.cpp   ← Task 8
tests/gpu/test_sprite_render.cpp       ← Task 12
CMakeLists.txt                          ← 修改 (新 option + add_subdirectory)
docs/PROGRESS.md                        ← Task 14
```

---

### Task 1: Win32 Window

打开/关闭一个 Win32 窗口,泵消息,暴露 `ShouldClose()` 与原生句柄给 RHI。窗口逻辑几乎全是 OS 调用,无法 WSL 单测,**用 Task 13 之前的临时空窗口 sandbox 目视验证**;本任务只交付 `Window` 类 + 编译通过。

**Files:**
- Create: `engine/platform/include/me/platform/Window.h`
- Create: `engine/platform/src/Window_Win32.cpp`
- Modify: `engine/platform/CMakeLists.txt`

**Interfaces:**
- Consumes: 无(仅 Win32 / 标准库)。
- Produces:
  - `struct me::platform::WindowDesc { int width=1280; int height=720; const char* title="MiniEngine"; }`
  - `class me::platform::Window`:
    - `static std::unique_ptr<Window> Create(const WindowDesc& desc);` // 失败返回 nullptr
    - `void PumpMessages();` // 处理本帧所有 Win32 消息
    - `bool ShouldClose() const;`
    - `int Width() const;` / `int Height() const;`
    - `void* NativeHandle() const;` // 返回 HWND(void* 以免在头里包含 windows.h)
    - 析构关闭窗口;拷贝禁用(持有 OS 资源)。

- [ ] **Step 1: 写头文件 `Window.h`**

```cpp
#pragma once

#include <memory>

namespace me::platform {

/** @brief 窗口创建参数(零魔法数字:默认值具名于此)。 */
struct WindowDesc {
    int width = 1280;
    int height = 720;
    const char* title = "MiniEngine";
};

/**
 * @brief 平台窗口(Win32 实现细节封死在 .cpp)。
 *
 * 不可拷贝:持有不可复制的 OS 窗口句柄。失败时工厂返回 nullptr(不抛异常)。
 */
class Window {
public:
    /** @brief 创建并显示窗口;失败返回 nullptr。 */
    static std::unique_ptr<Window> Create(const WindowDesc& desc);

    ~Window();
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    /** @brief 处理本帧累积的所有窗口消息(非阻塞)。 */
    void PumpMessages();
    /** @brief 用户是否请求关闭窗口。 */
    bool ShouldClose() const;

    int Width() const;
    int Height() const;

    /** @brief 原生窗口句柄(HWND,转为 void*);供 RHI 创建交换链使用。 */
    void* NativeHandle() const;

private:
    Window();
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace me::platform
```

- [ ] **Step 2: 写 `Window_Win32.cpp`**

```cpp
#include "me/platform/Window.h"

#include <windows.h>
#include <string>

namespace me::platform {

namespace {
/// 窗口类名(具名常量,避免裸字符串散落)。
constexpr wchar_t kWindowClassName[] = L"MiniEngineWindowClass";

std::wstring Widen(const char* utf8) {
    if (utf8 == nullptr) return std::wstring();
    int len = ::MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
    std::wstring out(len > 0 ? len - 1 : 0, L'\0');
    if (len > 1) ::MultiByteToWideChar(CP_UTF8, 0, utf8, -1, out.data(), len);
    return out;
}
} // namespace

struct Window::Impl {
    HWND hwnd = nullptr;
    int width = 0;
    int height = 0;
    bool shouldClose = false;
};

// 把 HWND 的 userdata 指向 Impl,从而在静态 WndProc 中取回实例状态。
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lparam);
        ::SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                            reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
        return ::DefWindowProcW(hwnd, msg, wparam, lparam);
    }
    auto* impl = reinterpret_cast<Window::Impl*>(
        ::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (impl != nullptr) {
        switch (msg) {
        case WM_CLOSE:
            impl->shouldClose = true;
            return 0;
        case WM_SIZE:
            impl->width = LOWORD(lparam);
            impl->height = HIWORD(lparam);
            return 0;
        }
    }
    return ::DefWindowProcW(hwnd, msg, wparam, lparam);
}

Window::Window() : m_impl(std::make_unique<Impl>()) {}
Window::~Window() {
    if (m_impl && m_impl->hwnd) ::DestroyWindow(m_impl->hwnd);
}

std::unique_ptr<Window> Window::Create(const WindowDesc& desc) {
    HINSTANCE hinst = ::GetModuleHandleW(nullptr);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = &WndProc;
    wc.hInstance = hinst;
    wc.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kWindowClassName;
    ::RegisterClassExW(&wc); // 重复注册返回 0 可忽略(同名类已存在)

    // 根据期望客户区尺寸反推窗口外框尺寸。
    RECT rc = {0, 0, desc.width, desc.height};
    const DWORD style = WS_OVERLAPPEDWINDOW;
    ::AdjustWindowRect(&rc, style, FALSE);

    std::unique_ptr<Window> self(new Window());
    self->m_impl->width = desc.width;
    self->m_impl->height = desc.height;

    HWND hwnd = ::CreateWindowExW(
        0, kWindowClassName, Widen(desc.title).c_str(), style,
        CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top,
        nullptr, nullptr, hinst, self->m_impl.get());
    if (hwnd == nullptr) {
        return nullptr; // 创建失败:不抛异常,返回 nullptr
    }
    self->m_impl->hwnd = hwnd;
    ::ShowWindow(hwnd, SW_SHOW);
    return self;
}

void Window::PumpMessages() {
    MSG msg = {};
    while (::PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        ::TranslateMessage(&msg);
        ::DispatchMessageW(&msg);
    }
}

bool Window::ShouldClose() const { return m_impl->shouldClose; }
int Window::Width() const { return m_impl->width; }
int Window::Height() const { return m_impl->height; }
void* Window::NativeHandle() const { return static_cast<void*>(m_impl->hwnd); }

} // namespace me::platform
```

- [ ] **Step 3: 更新 `engine/platform/CMakeLists.txt`,让 Win32 源仅在 Windows 编译**

```cmake
add_library(me_platform STATIC
    src/Platform.cpp
    src/Time.cpp
    src/FileSystem.cpp
)

if(WIN32)
    target_sources(me_platform PRIVATE
        src/Window_Win32.cpp
    )
endif()

target_include_directories(me_platform PUBLIC include)
target_compile_features(me_platform PUBLIC cxx_std_17)

# 单向依赖:platform → core(core 不得反向依赖 platform)
target_link_libraries(me_platform PUBLIC me_core)
```

- [ ] **Step 4: 验证编译 [WSL]**

`Window.h` 跨平台、`Window_Win32.cpp` 仅 Windows 编译,故 WSL 配置不受影响。
Run: `cmake -S . -B build-wsl -DME_BUILD_TESTS=ON -DME_BUILD_GPU_TESTS=OFF && cmake --build build-wsl -j`
Expected: 配置与编译成功(不含 Window_Win32.cpp)。

- [ ] **Step 5: 提交**

```bash
git add engine/platform/include/me/platform/Window.h engine/platform/src/Window_Win32.cpp engine/platform/CMakeLists.txt
git commit -m "feat(platform): Win32 Window(创建/消息泵/ShouldClose/HWND)"
```

> 真机目视(窗口能开能关)放到 Task 13 sandbox 一起验证,避免无桌面环境下测试窗口创建的脆弱性。

---

### Task 2: Input(跨平台状态机 + Win32 馈入)

把"键的按下/持续/抬起"做成**纯逻辑状态机**(可 WSL 单测),Win32 仅负责把消息翻译成 `KeyCode` 喂进去。

**Files:**
- Create: `engine/platform/include/me/platform/Input.h`
- Create: `engine/platform/src/Input.cpp` (跨平台状态机)
- Test: `tests/platform/test_input.cpp`
- Modify: `engine/platform/CMakeLists.txt`, `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: 无。
- Produces:
  - `enum class me::platform::KeyCode { Escape, Space, W, A, S, D, Count };`
  - `class me::platform::InputState`:
    - `void OnKeyDown(KeyCode k);` / `void OnKeyUp(KeyCode k);` // 由平台层调用
    - `void NewFrame();` // 每帧开始:把"本帧刚按下/刚抬起"清零,保留持续态
    - `bool IsDown(KeyCode k) const;` // 持续按住
    - `bool WasPressed(KeyCode k) const;` // 本帧刚按下(边沿)
    - `bool WasReleased(KeyCode k) const;` // 本帧刚抬起(边沿)

- [ ] **Step 1: 写失败测试 `tests/platform/test_input.cpp`**

```cpp
#include <doctest/doctest.h>
#include "me/platform/Input.h"

using me::platform::InputState;
using me::platform::KeyCode;

TEST_CASE("InputState 边沿与持续态") {
    InputState in;

    SUBCASE("按下当帧:IsDown 与 WasPressed 均真") {
        in.NewFrame();
        in.OnKeyDown(KeyCode::Space);
        CHECK(in.IsDown(KeyCode::Space));
        CHECK(in.WasPressed(KeyCode::Space));
        CHECK_FALSE(in.WasReleased(KeyCode::Space));
    }

    SUBCASE("下一帧持续按住:IsDown 真但 WasPressed 假") {
        in.NewFrame();
        in.OnKeyDown(KeyCode::Space);
        in.NewFrame(); // 进入下一帧,边沿清零
        CHECK(in.IsDown(KeyCode::Space));
        CHECK_FALSE(in.WasPressed(KeyCode::Space));
    }

    SUBCASE("抬起当帧:WasReleased 真,IsDown 假") {
        in.NewFrame();
        in.OnKeyDown(KeyCode::Escape);
        in.NewFrame();
        in.OnKeyUp(KeyCode::Escape);
        CHECK_FALSE(in.IsDown(KeyCode::Escape));
        CHECK(in.WasReleased(KeyCode::Escape));
    }
}
```

- [ ] **Step 2: 运行测试确认失败 [WSL]**

Run: `cmake --build build-wsl -j`
Expected: 编译失败(`Input.h` 不存在)。

- [ ] **Step 3: 写 `Input.h`**

```cpp
#pragma once

#include <array>
#include <cstddef>

namespace me::platform {

/** @brief M1 关心的最小键集合(零魔法数字:用枚举,不用裸 VK 码)。 */
enum class KeyCode { Escape, Space, W, A, S, D, Count };

/**
 * @brief 跨平台键盘状态机:持续态 + 本帧边沿(刚按下/刚抬起)。
 *
 * 平台层每帧先调用 NewFrame(),再把消息翻译成 OnKeyDown/OnKeyUp 喂入。
 * 纯逻辑、无 OS 依赖,可独立单测。
 */
class InputState {
public:
    /** @brief 每帧开始:清除上一帧的边沿标记,保留持续按住态。 */
    void NewFrame();

    void OnKeyDown(KeyCode k);
    void OnKeyUp(KeyCode k);

    bool IsDown(KeyCode k) const;       ///< 当前是否按住
    bool WasPressed(KeyCode k) const;   ///< 本帧是否刚按下(边沿)
    bool WasReleased(KeyCode k) const;  ///< 本帧是否刚抬起(边沿)

private:
    static constexpr std::size_t kCount = static_cast<std::size_t>(KeyCode::Count);
    std::array<bool, kCount> m_down{};       // 持续态
    std::array<bool, kCount> m_pressed{};    // 本帧边沿:按下
    std::array<bool, kCount> m_released{};   // 本帧边沿:抬起

    static std::size_t Index(KeyCode k) { return static_cast<std::size_t>(k); }
};

} // namespace me::platform
```

- [ ] **Step 4: 写 `Input.cpp`**

```cpp
#include "me/platform/Input.h"

namespace me::platform {

void InputState::NewFrame() {
    m_pressed.fill(false);
    m_released.fill(false);
}

void InputState::OnKeyDown(KeyCode k) {
    const std::size_t i = Index(k);
    if (!m_down[i]) {        // 仅在状态翻转时记录边沿,过滤系统的自动重复
        m_pressed[i] = true;
    }
    m_down[i] = true;
}

void InputState::OnKeyUp(KeyCode k) {
    const std::size_t i = Index(k);
    if (m_down[i]) {
        m_released[i] = true;
    }
    m_down[i] = false;
}

bool InputState::IsDown(KeyCode k) const { return m_down[Index(k)]; }
bool InputState::WasPressed(KeyCode k) const { return m_pressed[Index(k)]; }
bool InputState::WasReleased(KeyCode k) const { return m_released[Index(k)]; }

} // namespace me::platform
```

- [ ] **Step 5: 把源与测试加入构建**

`engine/platform/CMakeLists.txt` 的 `add_library` 主列表追加 `src/Input.cpp`(跨平台,不在 `if(WIN32)` 内)。
`tests/CMakeLists.txt` 的 `add_executable(me_tests ...)` 追加 `platform/test_input.cpp`。

- [ ] **Step 6: 运行测试确认通过 [WSL]**

Run: `cmake -S . -B build-wsl -DME_BUILD_TESTS=ON -DME_BUILD_GPU_TESTS=OFF && cmake --build build-wsl -j && ctest --test-dir build-wsl --output-on-failure`
Expected: PASS,全部用例绿。

- [ ] **Step 7: 提交**

```bash
git add engine/platform/include/me/platform/Input.h engine/platform/src/Input.cpp engine/platform/CMakeLists.txt tests/platform/test_input.cpp tests/CMakeLists.txt
git commit -m "feat(platform): Input 键盘状态机(持续态+边沿)+ 单测"
```

> Win32 消息→KeyCode 映射(`Input_Win32.cpp`)很薄且需真机,放到 Task 13 与 sandbox 主循环一起接线、目视验证(ESC 关窗、WASD 平移精灵)。

---

### Task 3: RHI 纯逻辑层 `me_rhi_cpu`(header-only,跨平台)

把"同步/帧环/描述符分配/四边形几何/精灵矩阵"做成**不含任何 DX12 头**的纯逻辑,在 WSL 用 doctest 红绿。这是把 GPU 代码里"可被抽离的算术"拎出来单测的关键(对应你选的测试策略)。

**Files:**
- Create: `engine/rhi/include/me/rhi/FenceTracker.h`
- Create: `engine/rhi/include/me/rhi/FrameRing.h`
- Create: `engine/rhi/include/me/rhi/DescriptorAllocatorLogic.h`
- Create: `engine/rhi/include/me/rhi/QuadGeometry.h`
- Create: `engine/rhi/include/me/rhi/SpriteTransform.h`
- Create: `engine/rhi/CMakeLists.txt`, `engine/rhi/README.md`
- Test: `tests/rhi/test_rhi_logic.cpp`
- Modify: `CMakeLists.txt`(新增 `option(ME_BUILD_GPU_TESTS ...)` 与 `add_subdirectory(engine/rhi)`)、`tests/CMakeLists.txt`

**Interfaces (Produces):**
- `struct me::rhi::FenceTracker { uint64_t NextSignalValue(); bool IsComplete(uint64_t completed, uint64_t target) const; }`
- `namespace me::rhi { constexpr uint32_t kFrameCount = 2; }`,`struct me::rhi::FrameRing { uint32_t Current() const; uint32_t Advance(); }`
- `struct me::rhi::DescriptorAllocatorLogic`:`DescriptorAllocatorLogic(uint32_t capacity, uint32_t increment)`、`std::optional<uint32_t> Allocate()`、`uint64_t CpuOffsetBytes(uint32_t index) const`、`uint32_t Count() const`、`uint32_t Capacity() const`
- `struct me::rhi::SpriteVertex { float x, y, u, v; }`,`std::array<SpriteVertex,4> UnitQuadVertices()`,`std::array<uint16_t,6> UnitQuadIndices()`,`constexpr uint32_t kSpriteVertexCount = 4; kSpriteIndexCount = 6;`
- `me::Matrix4x4 me::rhi::MakeSpriteModelMatrix(me::Vector2 position, me::Vector2 size, float rotationRadians)`

- [ ] **Step 1: 写失败测试 `tests/rhi/test_rhi_logic.cpp`**

```cpp
#include <doctest/doctest.h>

#include "me/rhi/FenceTracker.h"
#include "me/rhi/FrameRing.h"
#include "me/rhi/DescriptorAllocatorLogic.h"
#include "me/rhi/QuadGeometry.h"
#include "me/rhi/SpriteTransform.h"

using namespace me::rhi;

TEST_CASE("FenceTracker 递增信号值且单调完成判定") {
    FenceTracker t;
    const uint64_t a = t.NextSignalValue();
    const uint64_t b = t.NextSignalValue();
    CHECK(b == a + 1);
    CHECK_FALSE(t.IsComplete(a - 1, a)); // GPU 还没到 a
    CHECK(t.IsComplete(a, a));           // 刚好到 a
    CHECK(t.IsComplete(b, a));           // 超过 a
}

TEST_CASE("FrameRing 在 kFrameCount 内循环") {
    FrameRing ring;
    CHECK(ring.Current() == 0u);
    CHECK(ring.Advance() == 1u % kFrameCount);
    // 推进 kFrameCount 次回到起点
    FrameRing r2;
    for (uint32_t i = 0; i < kFrameCount; ++i) r2.Advance();
    CHECK(r2.Current() == 0u);
}

TEST_CASE("DescriptorAllocatorLogic 线性分配 + 容量上限 + 字节偏移") {
    constexpr uint32_t kIncrement = 32;
    DescriptorAllocatorLogic alloc(2, kIncrement);
    auto i0 = alloc.Allocate();
    auto i1 = alloc.Allocate();
    auto i2 = alloc.Allocate();
    REQUIRE(i0.has_value());
    REQUIRE(i1.has_value());
    CHECK(*i0 == 0u);
    CHECK(*i1 == 1u);
    CHECK_FALSE(i2.has_value());                 // 满了返回 nullopt
    CHECK(alloc.CpuOffsetBytes(*i1) == kIncrement);
    CHECK(alloc.Count() == 2u);
}

TEST_CASE("UnitQuad 顶点/索引") {
    auto v = UnitQuadVertices();
    auto idx = UnitQuadIndices();
    CHECK(v.size() == kSpriteVertexCount);
    CHECK(idx.size() == kSpriteIndexCount);
    // 单位四边形居中:四角在 ±0.5
    CHECK(v[0].x == doctest::Approx(-0.5f));
    CHECK(v[0].y == doctest::Approx(-0.5f));
    CHECK(v[2].x == doctest::Approx(0.5f));
    CHECK(v[2].y == doctest::Approx(0.5f));
    // 左下角 UV 的 v 分量在底部(=1,纹理行向下)
    CHECK(v[0].u == doctest::Approx(0.0f));
    CHECK(v[0].v == doctest::Approx(1.0f));
}

TEST_CASE("MakeSpriteModelMatrix 把单位四边形角点映射到世界矩形") {
    using me::Vector2;
    // 位置 (10,20),尺寸 (4,6),无旋转
    auto model = MakeSpriteModelMatrix(Vector2{10.0f, 20.0f}, Vector2{4.0f, 6.0f}, 0.0f);
    // 右上角局部 (0.5,0.5) → 世界 (10+2, 20+3) = (12,23)
    Vector2 topRight = model.TransformPoint(Vector2{0.5f, 0.5f});
    CHECK(topRight.x == doctest::Approx(12.0f));
    CHECK(topRight.y == doctest::Approx(23.0f));
    // 中心 (0,0) → 世界 (10,20)
    Vector2 center = model.TransformPoint(Vector2{0.0f, 0.0f});
    CHECK(center.x == doctest::Approx(10.0f));
    CHECK(center.y == doctest::Approx(20.0f));
}
```

- [ ] **Step 2: 运行测试确认失败 [WSL]**

Run: `cmake -S . -B build-wsl -DME_BUILD_TESTS=ON -DME_BUILD_GPU_TESTS=OFF && cmake --build build-wsl -j`
Expected: 编译失败(`me/rhi/*.h` 不存在)。

- [ ] **Step 3: 写 `FenceTracker.h`**

```cpp
#pragma once

#include <cstdint>

namespace me::rhi {

/**
 * @brief GPU 围栏的纯逻辑部分:产生单调递增的信号值并判定是否已完成。
 *
 * 不含任何 DX12 依赖;真正的 ID3D12Fence 在 Fence 类里持有,调用本结构取值。
 * 这是 DX12 同步模型的核心:CPU 给队列排一个递增的 fence 值,GPU 执行到时回写,
 * CPU 比较 completedValue >= target 即知该批命令已结束。
 */
struct FenceTracker {
    /** @brief 取下一个待排队的信号值(从 1 开始,0 表示"尚未提交")。 */
    uint64_t NextSignalValue() { return m_next++; }

    /** @brief GPU 已完成值 completed 是否达到/超过目标 target。 */
    bool IsComplete(uint64_t completed, uint64_t target) const {
        return completed >= target;
    }

private:
    uint64_t m_next = 1;
};

} // namespace me::rhi
```

- [ ] **Step 4: 写 `FrameRing.h`**

```cpp
#pragma once

#include <cstdint>

namespace me::rhi {

/// 飞行中的帧数(双缓冲)。具名常量,贯穿命令分配器/围栏值数组尺寸。
constexpr uint32_t kFrameCount = 2;

/** @brief 在 [0, kFrameCount) 间循环的帧索引(选当前命令分配器/资源槽)。 */
struct FrameRing {
    uint32_t Current() const { return m_index; }
    /** @brief 推进到下一帧并返回新索引。 */
    uint32_t Advance() {
        m_index = (m_index + 1) % kFrameCount;
        return m_index;
    }

private:
    uint32_t m_index = 0;
};

} // namespace me::rhi
```

- [ ] **Step 5: 写 `DescriptorAllocatorLogic.h`**

```cpp
#pragma once

#include <cstdint>
#include <optional>

namespace me::rhi {

/**
 * @brief 描述符堆的线性分配逻辑(纯算术,不持有 ID3D12DescriptorHeap)。
 *
 * 真正的 DescriptorHeap 把堆起始 CPU/GPU 句柄 + 本逻辑组合:Allocate() 给出
 * 槽位序号,再用 CpuOffsetBytes(index) 加到堆起始句柄上得到具体描述符位置。
 * increment 来自 device->GetDescriptorHandleIncrementSize(heapType),运行时查询。
 */
struct DescriptorAllocatorLogic {
    DescriptorAllocatorLogic(uint32_t capacity, uint32_t increment)
        : m_capacity(capacity), m_increment(increment) {}

    /** @brief 分配一个槽位序号;堆已满返回 std::nullopt。 */
    std::optional<uint32_t> Allocate() {
        if (m_count >= m_capacity) return std::nullopt;
        return m_count++;
    }

    /** @brief 槽位序号 → 相对堆起始的字节偏移。 */
    uint64_t CpuOffsetBytes(uint32_t index) const {
        return static_cast<uint64_t>(index) * m_increment;
    }

    uint32_t Count() const { return m_count; }
    uint32_t Capacity() const { return m_capacity; }

private:
    uint32_t m_capacity;
    uint32_t m_increment;
    uint32_t m_count = 0;
};

} // namespace me::rhi
```

- [ ] **Step 6: 写 `QuadGeometry.h`**

```cpp
#pragma once

#include <array>
#include <cstdint>

namespace me::rhi {

/// 精灵顶点:局部坐标 (x,y) + 纹理坐标 (u,v)。布局须与 sprite.hlsl 的输入一致。
struct SpriteVertex {
    float x, y; // 局部空间,单位四边形居中于原点,角点在 ±0.5
    float u, v; // 纹理坐标,左上 (0,0) 右下 (1,1)
};

constexpr uint32_t kSpriteVertexCount = 4;
constexpr uint32_t kSpriteIndexCount = 6;

/**
 * @brief 居中单位四边形的 4 个顶点(局部 ±0.5)。
 *
 * 世界 Y 向上,纹理 V 向下:故局部底边 (y=-0.5) 对应 v=1,顶边 (y=+0.5) 对应 v=0,
 * 使贴图正立显示。索引顺序见 UnitQuadIndices;M1 关闭背面剔除,绕序无关。
 */
inline std::array<SpriteVertex, kSpriteVertexCount> UnitQuadVertices() {
    return {{
        {-0.5f, -0.5f, 0.0f, 1.0f}, // 0 左下
        { 0.5f, -0.5f, 1.0f, 1.0f}, // 1 右下
        { 0.5f,  0.5f, 1.0f, 0.0f}, // 2 右上
        {-0.5f,  0.5f, 0.0f, 0.0f}, // 3 左上
    }};
}

/** @brief 两个三角形:0-1-2 与 0-2-3。 */
inline std::array<uint16_t, kSpriteIndexCount> UnitQuadIndices() {
    return {{0, 1, 2, 0, 2, 3}};
}

} // namespace me::rhi
```

- [ ] **Step 7: 写 `SpriteTransform.h`**

```cpp
#pragma once

#include "me/core/Matrix4x4.h"
#include "me/core/Vector2.h"

namespace me::rhi {

/**
 * @brief 由位置/尺寸/旋转构造精灵的世界模型矩阵(行向量约定 v' = v*M)。
 *
 * 顺序:先缩放到尺寸,再旋转,最后平移到位置 —— model = S * R * T。
 * 单位四边形角点 (±0.5) 经此矩阵落到世界矩形 [pos - size/2, pos + size/2]。
 */
inline me::Matrix4x4 MakeSpriteModelMatrix(me::Vector2 position,
                                           me::Vector2 size,
                                           float rotationRadians) {
    const me::Matrix4x4 s = me::Matrix4x4::Scale(size);
    const me::Matrix4x4 r = me::Matrix4x4::Rotation(rotationRadians);
    const me::Matrix4x4 t = me::Matrix4x4::Translation(position);
    return s * r * t; // 行向量:v * S * R * T
}

} // namespace me::rhi
```

- [ ] **Step 8: 写 `engine/rhi/CMakeLists.txt`(本任务只建立 header-only 子目标)**

```cmake
# 纯逻辑、跨平台、header-only:RHI 中可脱离 DX12 单测的部分。
add_library(me_rhi_cpu INTERFACE)
target_include_directories(me_rhi_cpu INTERFACE include)
target_compile_features(me_rhi_cpu INTERFACE cxx_std_17)
target_link_libraries(me_rhi_cpu INTERFACE me_core)

# DX12 实现层 me_rhi 仅在 Windows 构建(后续任务逐步填充源文件)。
if(WIN32)
    add_library(me_rhi STATIC
        # 源文件在 Task 5/6/7/8/9/11 中追加
    )
    target_include_directories(me_rhi PUBLIC include)
    target_compile_features(me_rhi PUBLIC cxx_std_17)
    target_link_libraries(me_rhi
        PUBLIC me_core me_platform me_rhi_cpu
        PRIVATE d3d12 dxgi d3dcompiler dxguid)
    # 让运行时能定位着色器/纹理:把仓库 assets 目录编进宏。
    target_compile_definitions(me_rhi PUBLIC ME_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets")
endif()
```

> 注意:`add_library(me_rhi STATIC)` 不能有空源列表,会 CMake 报错。Task 5 添加第一个 `.cpp` 之前,本步先用占位:在 `if(WIN32)` 块内暂时改为 `add_library(me_rhi STATIC src/GpuDevice.cpp)` 并在 Task 5 创建该文件;若想本任务即编过 Windows 侧,可临时加 `src/RhiPlaceholder.cpp`(内容仅 `namespace me::rhi {}`)。WSL 构建不受影响(`me_rhi` 不参与)。

- [ ] **Step 9: 写 `engine/rhi/README.md`**

```markdown
# engine/rhi(me_rhi / me_rhi_cpu)

DX12 薄封装。裸 `ID3D12*` 封死在本模块内,不外泄。

- **me_rhi_cpu**(INTERFACE,跨平台):同步/帧环/描述符/几何/精灵矩阵等纯逻辑,可在 WSL 单测。
- **me_rhi**(STATIC,仅 Windows):Device / Fence / CommandContext / SwapChain / GpuBuffer /
  GpuTexture / Shader / 像素回读。依赖系统 DX12(d3d12/dxgi/d3dcompiler)。

依赖:Core, Platform, me_rhi_cpu(单向)。GPU 正确性由 `me_gpu_tests`(WARP 回读)把关。
```

- [ ] **Step 10: 接好顶层 CMake 与测试**

`CMakeLists.txt`(根)在 `option(ME_BUILD_TESTS ...)` 之后新增:
```cmake
option(ME_BUILD_GPU_TESTS "Build DX12/WARP GPU tests (Windows only)" OFF)

# MSVC 必须 /utf-8:源文件含中文注释,否则 GBK 误读会编译失败(见 Global Constraints)。
add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
```
并在 `add_subdirectory(engine/platform)` 之后新增 `add_subdirectory(engine/rhi)`。
`tests/CMakeLists.txt` 的 `me_tests` 源列表追加 `rhi/test_rhi_logic.cpp`,并在 `target_link_libraries(me_tests ...)` 追加 `me_rhi_cpu`。

- [ ] **Step 11: 运行测试确认通过 [WSL]**

Run: `cmake -S . -B build-wsl -DME_BUILD_TESTS=ON -DME_BUILD_GPU_TESTS=OFF && cmake --build build-wsl -j && ctest --test-dir build-wsl --output-on-failure`
Expected: PASS,新增 5 个 TEST_CASE 全绿。

- [ ] **Step 12: 提交**

```bash
git add engine/rhi tests/rhi/test_rhi_logic.cpp tests/CMakeLists.txt CMakeLists.txt
git commit -m "feat(rhi): me_rhi_cpu 纯逻辑(fence/frame/descriptor/quad/transform)+ 单测"
```

---

### Task 4: DX12 公共设施 `D3DCommon.h`(`ME_HR_CHECK` + ComPtr 别名)

DX12 调用返回 `HRESULT`;按规范用 `ME_HR_CHECK` 把失败转成"不变量违反 → 中止"(M1 阶段初始化失败视为致命,不做软恢复)。仅 Windows。

**Files:**
- Create: `engine/rhi/include/me/rhi/D3DCommon.h`
- Create: `engine/rhi/src/D3DCommon.cpp`
- Modify: `engine/rhi/CMakeLists.txt`(把 `D3DCommon.cpp` 加进 `me_rhi`,替换 Task 3 的占位源)

**Interfaces (Produces):**
- `me::rhi::ComPtr<T>` = `Microsoft::WRL::ComPtr<T>` 别名。
- 宏 `ME_HR_CHECK(expr)`:`expr` 的 `HRESULT` 若 `FAILED`,记录 `file:line` + hr 后 `std::abort()`。
- `void me::rhi::detail::HrFail(long hr, const char* expr, const char* file, int line);`

- [ ] **Step 1: 写 `D3DCommon.h`**

```cpp
#pragma once

#include <wrl/client.h> // Microsoft::WRL::ComPtr

namespace me::rhi {

/// 智能 COM 指针别名:RAII 释放 ID3D12* / IDXGI* 等。
template <typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

namespace detail {
/** @brief HRESULT 失败处理:打印诊断后中止(不变量违反路径)。 */
[[noreturn]] void HrFail(long hr, const char* expr, const char* file, int line);
} // namespace detail

} // namespace me::rhi

/** @brief 检查 DX12 调用的 HRESULT;FAILED 则中止。表达式仅求值一次。 */
#define ME_HR_CHECK(expr)                                                      \
    do {                                                                       \
        const HRESULT _meHr = (expr);                                          \
        if (FAILED(_meHr)) {                                                   \
            ::me::rhi::detail::HrFail(static_cast<long>(_meHr), #expr,         \
                                      __FILE__, __LINE__);                     \
        }                                                                      \
    } while (false)
```

- [ ] **Step 2: 写 `D3DCommon.cpp`**

```cpp
#include "me/rhi/D3DCommon.h"

#include <cstdio>
#include <cstdlib>

namespace me::rhi::detail {

void HrFail(long hr, const char* expr, const char* file, int line) {
    std::fprintf(stderr, "[ME_HR_CHECK] FAILED hr=0x%08lX  %s\n  at %s:%d\n",
                 static_cast<unsigned long>(hr), expr, file, line);
    std::fflush(stderr);
    std::abort();
}

} // namespace me::rhi::detail
```

- [ ] **Step 3: 让 `me_rhi` 编译此文件**

在 `engine/rhi/CMakeLists.txt` 的 `if(WIN32)` 块里,把 `add_library(me_rhi STATIC ...)` 的源改为(去掉占位):
```cmake
    add_library(me_rhi STATIC
        src/D3DCommon.cpp
    )
```
(后续任务继续往这个列表追加源文件。)

- [ ] **Step 4: 验证 [WSL]**

仅确认 WSL 构建仍不受影响(`me_rhi` 不参与):
Run: `cmake --build build-wsl -j`
Expected: 成功(无变化)。Windows 侧编译留待 Task 5 一并验证。

- [ ] **Step 5: 提交**

```bash
git add engine/rhi/include/me/rhi/D3DCommon.h engine/rhi/src/D3DCommon.cpp engine/rhi/CMakeLists.txt
git commit -m "feat(rhi): D3DCommon(ME_HR_CHECK + ComPtr 别名)"
```

---

### Task 5: GpuDevice + Fence + WARP 测试骨架(第一个 GPU 红绿)

创建 DX12 设备(支持 WARP 软件适配器)、直接命令队列与围栏,跑通"signal→wait→completed 值前进"的同步闭环。这是 M1 学习重点。**首次引入 Windows-only 的 `me_gpu_tests` 目标。**

**Files:**
- Create: `engine/rhi/include/me/rhi/GpuDevice.h`, `engine/rhi/src/GpuDevice.cpp`
- Create: `engine/rhi/include/me/rhi/Fence.h`, `engine/rhi/src/Fence.cpp`
- Create: `tests/gpu/gpu_test_main.cpp`, `tests/gpu/test_device_fence.cpp`
- Modify: `engine/rhi/CMakeLists.txt`, `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `FenceTracker`(Task 3),`ME_HR_CHECK`/`ComPtr`(Task 4)。
- Produces:
  - `class me::rhi::GpuDevice`:
    - `static std::unique_ptr<GpuDevice> Create(bool useWarp);` // 失败返回 nullptr
    - `ID3D12Device* Device() const;`
    - `ID3D12CommandQueue* Queue() const;` // DIRECT 队列
  - `class me::rhi::Fence`:
    - `static std::unique_ptr<Fence> Create(ID3D12Device* device);`
    - `uint64_t Signal(ID3D12CommandQueue* queue);` // 排入一个递增信号值,返回该值
    - `void Wait(uint64_t value);` // 阻塞直到 GPU 完成该值
    - `void Flush(ID3D12CommandQueue* queue);` // Signal + Wait,清空队列
    - `uint64_t CompletedValue() const;`

- [ ] **Step 1: 写失败测试 `tests/gpu/test_device_fence.cpp`**

```cpp
#include <doctest/doctest.h>

#include "me/rhi/GpuDevice.h"
#include "me/rhi/Fence.h"

using namespace me::rhi;

TEST_CASE("WARP 设备可创建,围栏 signal/wait 推进完成值") {
    auto device = GpuDevice::Create(/*useWarp=*/true);
    REQUIRE(device != nullptr);
    REQUIRE(device->Device() != nullptr);
    REQUIRE(device->Queue() != nullptr);

    auto fence = Fence::Create(device->Device());
    REQUIRE(fence != nullptr);

    const uint64_t before = fence->CompletedValue();
    const uint64_t target = fence->Signal(device->Queue());
    fence->Wait(target);
    CHECK(fence->CompletedValue() >= target);
    CHECK(fence->CompletedValue() > before);

    // Flush 再来一次,确保可重复同步。
    fence->Flush(device->Queue());
}
```

- [ ] **Step 2: 写 `gpu_test_main.cpp`(doctest 入口,与 me_tests 分离)**

```cpp
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
```

- [ ] **Step 3: 写 `GpuDevice.h`**

```cpp
#pragma once

#include <memory>
#include <d3d12.h>
#include <dxgi1_6.h>

#include "me/rhi/D3DCommon.h"

namespace me::rhi {

/**
 * @brief DX12 设备 + DIRECT 命令队列。裸 ID3D12* 仅经只读取用器外泄给同模块其它类。
 *
 * Create(useWarp=true) 选用 WARP 软件光栅器,使无独显/无头环境(测试)也能跑。
 */
class GpuDevice {
public:
    /** @brief 创建设备与命令队列;失败返回 nullptr(已记录原因)。 */
    static std::unique_ptr<GpuDevice> Create(bool useWarp);

    ID3D12Device* Device() const { return m_device.Get(); }
    ID3D12CommandQueue* Queue() const { return m_queue.Get(); }
    IDXGIFactory4* Factory() const { return m_factory.Get(); }

private:
    ComPtr<IDXGIFactory4> m_factory;
    ComPtr<ID3D12Device> m_device;
    ComPtr<ID3D12CommandQueue> m_queue;
};

} // namespace me::rhi
```

- [ ] **Step 4: 写 `GpuDevice.cpp`**

```cpp
#include "me/rhi/GpuDevice.h"

#include "me/core/Log.h"

namespace me::rhi {

std::unique_ptr<GpuDevice> GpuDevice::Create(bool useWarp) {
    UINT factoryFlags = 0;
#if !defined(NDEBUG)
    // 开启 DX12 调试层,把 API 误用打到输出窗口。
    {
        ComPtr<ID3D12Debug> debug;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) {
            debug->EnableDebugLayer();
            factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }
    }
#endif

    auto self = std::make_unique<GpuDevice>();
    if (FAILED(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&self->m_factory)))) {
        ME_LOG_ERROR("CreateDXGIFactory2 失败");
        return nullptr;
    }

    // 选择适配器:WARP(软件)或第一个硬件适配器。
    ComPtr<IDXGIAdapter1> adapter;
    if (useWarp) {
        if (FAILED(self->m_factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter)))) {
            ME_LOG_ERROR("EnumWarpAdapter 失败");
            return nullptr;
        }
    } else {
        for (UINT i = 0; self->m_factory->EnumAdapters1(i, &adapter) !=
                         DXGI_ERROR_NOT_FOUND; ++i) {
            DXGI_ADAPTER_DESC1 desc{};
            adapter->GetDesc1(&desc);
            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue; // 跳过软件适配器
            if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0,
                                            __uuidof(ID3D12Device), nullptr))) {
                break;
            }
        }
    }
    if (adapter == nullptr) {
        ME_LOG_ERROR("未找到可用 DX12 适配器");
        return nullptr;
    }

    if (FAILED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0,
                                 IID_PPV_ARGS(&self->m_device)))) {
        ME_LOG_ERROR("D3D12CreateDevice 失败");
        return nullptr;
    }

    D3D12_COMMAND_QUEUE_DESC qd = {};
    qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    qd.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    if (FAILED(self->m_device->CreateCommandQueue(&qd, IID_PPV_ARGS(&self->m_queue)))) {
        ME_LOG_ERROR("CreateCommandQueue 失败");
        return nullptr;
    }
    return self;
}

} // namespace me::rhi
```

- [ ] **Step 5: 写 `Fence.h`**

```cpp
#pragma once

#include <cstdint>
#include <memory>
#include <d3d12.h>

#include "me/rhi/D3DCommon.h"
#include "me/rhi/FenceTracker.h"

namespace me::rhi {

/**
 * @brief CPU↔GPU 同步围栏:把递增信号值排进队列,并能阻塞等待其完成。
 *
 * 复用 FenceTracker 产生单调信号值;Win32 事件用于 Wait 时挂起 CPU 线程。
 */
class Fence {
public:
    static std::unique_ptr<Fence> Create(ID3D12Device* device);
    ~Fence();
    Fence(const Fence&) = delete;
    Fence& operator=(const Fence&) = delete;

    /** @brief 在队列尾排入一个新信号值并返回它。 */
    uint64_t Signal(ID3D12CommandQueue* queue);
    /** @brief 阻塞 CPU 直到 GPU 完成给定信号值。 */
    void Wait(uint64_t value);
    /** @brief Signal + Wait:清空队列(M1 简单同步,每帧用)。 */
    void Flush(ID3D12CommandQueue* queue) { Wait(Signal(queue)); }

    uint64_t CompletedValue() const { return m_fence->GetCompletedValue(); }

private:
    Fence() = default;
    ComPtr<ID3D12Fence> m_fence;
    void* m_event = nullptr; // HANDLE
    FenceTracker m_tracker;
};

} // namespace me::rhi
```

- [ ] **Step 6: 写 `Fence.cpp`**

```cpp
#include "me/rhi/Fence.h"

#include <windows.h>

namespace me::rhi {

std::unique_ptr<Fence> Fence::Create(ID3D12Device* device) {
    auto self = std::unique_ptr<Fence>(new Fence());
    if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE,
                                   IID_PPV_ARGS(&self->m_fence)))) {
        return nullptr;
    }
    self->m_event = ::CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (self->m_event == nullptr) {
        return nullptr;
    }
    return self;
}

Fence::~Fence() {
    if (m_event) ::CloseHandle(static_cast<HANDLE>(m_event));
}

uint64_t Fence::Signal(ID3D12CommandQueue* queue) {
    const uint64_t value = m_tracker.NextSignalValue();
    ME_HR_CHECK(queue->Signal(m_fence.Get(), value));
    return value;
}

void Fence::Wait(uint64_t value) {
    if (m_tracker.IsComplete(m_fence->GetCompletedValue(), value)) {
        return; // 已完成,无需挂起
    }
    ME_HR_CHECK(m_fence->SetEventOnCompletion(value, static_cast<HANDLE>(m_event)));
    ::WaitForSingleObject(static_cast<HANDLE>(m_event), INFINITE);
}

} // namespace me::rhi
```

- [ ] **Step 7: 把源加入 `me_rhi`,并在 `engine/rhi/CMakeLists.txt` 增加 `me_gpu_tests`**

`me_rhi` 源列表追加 `src/GpuDevice.cpp`、`src/Fence.cpp`。在 `engine/rhi/CMakeLists.txt` 的 `if(WIN32)` 块末尾追加 GPU 测试目标(由根选项 `ME_BUILD_GPU_TESTS` 控制):
```cmake
    if(ME_BUILD_GPU_TESTS)
        add_executable(me_gpu_tests
            ${CMAKE_SOURCE_DIR}/tests/gpu/gpu_test_main.cpp
            ${CMAKE_SOURCE_DIR}/tests/gpu/test_device_fence.cpp
            # 后续 Task 8/12 追加 test_texture_roundtrip.cpp / test_sprite_render.cpp
        )
        target_link_libraries(me_gpu_tests PRIVATE doctest::doctest me_rhi me_rhi_cpu)
        add_test(NAME me_gpu_tests COMMAND me_gpu_tests)
    endif()
```

> `me_gpu_tests` 的源用绝对路径引用 `tests/gpu/*`,因为该目标定义在 `engine/rhi/` 子目录;这样能与 `me_renderer`(Task 12)产物链接而不被 `tests/` 的跨平台约束牵连。Task 12 会把该目标的链接库追加 `me_renderer`、源追加 `test_sprite_render.cpp`。

- [ ] **Step 8: 在 Windows 上运行测试确认通过 [Windows]**

Run (Developer PowerShell):
```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DME_BUILD_TESTS=ON -DME_BUILD_GPU_TESTS=ON
cmake --build build --config Debug
ctest --test-dir build -C Debug -R me_gpu_tests --output-on-failure
```
Expected: `me_gpu_tests` PASS —— WARP 设备创建成功,围栏完成值前进。

- [ ] **Step 9: 提交**

```bash
git add engine/rhi tests/gpu/gpu_test_main.cpp tests/gpu/test_device_fence.cpp
git commit -m "feat(rhi): GpuDevice(WARP/硬件)+ Fence 同步 + WARP GPU 测试骨架"
```

---

### Task 6: CommandContext + DescriptorHeap

封装命令分配器/命令列表的"录制一帧"流程,以及描述符堆(RTV 与 CBV/SRV/UAV)的分配。这些被回读、精灵渲染、交换链共用。

**Files:**
- Create: `engine/rhi/include/me/rhi/CommandContext.h`, `engine/rhi/src/CommandContext.cpp`
- Create: `engine/rhi/include/me/rhi/DescriptorHeap.h`, `engine/rhi/src/DescriptorHeap.cpp`
- Modify: `engine/rhi/CMakeLists.txt`(追加两个 `.cpp`)

**Interfaces:**
- Consumes: `FrameRing`/`kFrameCount`、`DescriptorAllocatorLogic`(Task 3),`GpuDevice`(Task 5)。
- Produces:
  - `class me::rhi::CommandContext`:
    - `static std::unique_ptr<CommandContext> Create(ID3D12Device* device);`
    - `ID3D12GraphicsCommandList* Begin();` // reset 当前帧分配器+列表,返回处于录制态的列表
    - `void End();` // 关闭列表
    - `ID3D12GraphicsCommandList* List() const;`
    - `void Execute(ID3D12CommandQueue* queue);` // ExecuteCommandLists(单个)
    - `void AdvanceFrame();` // 推进 FrameRing(每 Present 后)
  - `struct me::rhi::Descriptor { D3D12_CPU_DESCRIPTOR_HANDLE cpu; D3D12_GPU_DESCRIPTOR_HANDLE gpu; uint32_t index; };`
  - `class me::rhi::DescriptorHeap`:
    - `static std::unique_ptr<DescriptorHeap> Create(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t capacity, bool shaderVisible);`
    - `Descriptor Allocate();` // 容量耗尽时 ME_ASSERT
    - `ID3D12DescriptorHeap* Heap() const;`

- [ ] **Step 1: 写 `DescriptorHeap.h`**

```cpp
#pragma once

#include <cstdint>
#include <memory>
#include <d3d12.h>

#include "me/rhi/D3DCommon.h"
#include "me/rhi/DescriptorAllocatorLogic.h"

namespace me::rhi {

/** @brief 一个已分配描述符的 CPU/GPU 句柄与槽位序号。 */
struct Descriptor {
    D3D12_CPU_DESCRIPTOR_HANDLE cpu{};
    D3D12_GPU_DESCRIPTOR_HANDLE gpu{}; // 仅 shaderVisible 堆有效
    uint32_t index = 0;
};

/**
 * @brief 线性描述符堆:封装 ID3D12DescriptorHeap + 分配逻辑(DescriptorAllocatorLogic)。
 *
 * RTV 堆用非着色器可见;SRV(贴图)堆用着色器可见,以便着色阶段经描述符表采样。
 */
class DescriptorHeap {
public:
    static std::unique_ptr<DescriptorHeap> Create(ID3D12Device* device,
                                                  D3D12_DESCRIPTOR_HEAP_TYPE type,
                                                  uint32_t capacity,
                                                  bool shaderVisible);
    /** @brief 取下一个描述符槽;堆满则 ME_ASSERT(编程错误)。 */
    Descriptor Allocate();
    ID3D12DescriptorHeap* Heap() const { return m_heap.Get(); }

private:
    DescriptorHeap(uint32_t capacity, uint32_t increment)
        : m_logic(capacity, increment) {}

    ComPtr<ID3D12DescriptorHeap> m_heap;
    DescriptorAllocatorLogic m_logic;
    D3D12_CPU_DESCRIPTOR_HANDLE m_cpuStart{};
    D3D12_GPU_DESCRIPTOR_HANDLE m_gpuStart{};
    bool m_shaderVisible = false;
};

} // namespace me::rhi
```

- [ ] **Step 2: 写 `DescriptorHeap.cpp`**

```cpp
#include "me/rhi/DescriptorHeap.h"

#include "me/core/Assert.h"

namespace me::rhi {

std::unique_ptr<DescriptorHeap> DescriptorHeap::Create(
    ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type,
    uint32_t capacity, bool shaderVisible) {

    const uint32_t increment = device->GetDescriptorHandleIncrementSize(type);
    auto self = std::unique_ptr<DescriptorHeap>(new DescriptorHeap(capacity, increment));
    self->m_shaderVisible = shaderVisible;

    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = type;
    desc.NumDescriptors = capacity;
    desc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
                               : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    if (FAILED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&self->m_heap)))) {
        return nullptr;
    }
    self->m_cpuStart = self->m_heap->GetCPUDescriptorHandleForHeapStart();
    if (shaderVisible) {
        self->m_gpuStart = self->m_heap->GetGPUDescriptorHandleForHeapStart();
    }
    return self;
}

Descriptor DescriptorHeap::Allocate() {
    auto slot = m_logic.Allocate();
    ME_ASSERT_MSG(slot.has_value(), "描述符堆已满");
    const uint64_t offset = m_logic.CpuOffsetBytes(*slot);

    Descriptor d{};
    d.index = *slot;
    d.cpu.ptr = m_cpuStart.ptr + offset;
    if (m_shaderVisible) {
        d.gpu.ptr = m_gpuStart.ptr + offset;
    }
    return d;
}

} // namespace me::rhi
```

- [ ] **Step 3: 写 `CommandContext.h`**

```cpp
#pragma once

#include <array>
#include <memory>
#include <d3d12.h>

#include "me/rhi/D3DCommon.h"
#include "me/rhi/FrameRing.h"

namespace me::rhi {

/**
 * @brief 每帧命令录制:kFrameCount 个命令分配器 + 一个图形命令列表。
 *
 * Begin() 重置当前帧分配器与列表并返回录制态列表;End() 关闭;Execute() 提交。
 * 双缓冲下,只要等待对应帧的围栏完成即可安全重置该帧分配器。
 */
class CommandContext {
public:
    static std::unique_ptr<CommandContext> Create(ID3D12Device* device);

    ID3D12GraphicsCommandList* Begin();
    void End();
    ID3D12GraphicsCommandList* List() const { return m_list.Get(); }
    void Execute(ID3D12CommandQueue* queue);
    void AdvanceFrame() { m_ring.Advance(); }
    uint32_t FrameIndex() const { return m_ring.Current(); }

private:
    CommandContext() = default;
    std::array<ComPtr<ID3D12CommandAllocator>, kFrameCount> m_allocators;
    ComPtr<ID3D12GraphicsCommandList> m_list;
    FrameRing m_ring;
};

} // namespace me::rhi
```

- [ ] **Step 4: 写 `CommandContext.cpp`**

```cpp
#include "me/rhi/CommandContext.h"

namespace me::rhi {

std::unique_ptr<CommandContext> CommandContext::Create(ID3D12Device* device) {
    auto self = std::unique_ptr<CommandContext>(new CommandContext());
    for (auto& alloc : self->m_allocators) {
        if (FAILED(device->CreateCommandAllocator(
                D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&alloc)))) {
            return nullptr;
        }
    }
    if (FAILED(device->CreateCommandList(
            0, D3D12_COMMAND_LIST_TYPE_DIRECT,
            self->m_allocators[0].Get(), nullptr, IID_PPV_ARGS(&self->m_list)))) {
        return nullptr;
    }
    self->m_list->Close(); // 创建即处于录制态,先关闭以便 Begin 统一 Reset
    return self;
}

ID3D12GraphicsCommandList* CommandContext::Begin() {
    auto& alloc = m_allocators[m_ring.Current()];
    ME_HR_CHECK(alloc->Reset());
    ME_HR_CHECK(m_list->Reset(alloc.Get(), nullptr));
    return m_list.Get();
}

void CommandContext::End() {
    ME_HR_CHECK(m_list->Close());
}

void CommandContext::Execute(ID3D12CommandQueue* queue) {
    ID3D12CommandList* lists[] = {m_list.Get()};
    queue->ExecuteCommandLists(1, lists);
}

} // namespace me::rhi
```

- [ ] **Step 5: 加入 `me_rhi` 源列表并 Windows 编译验证 [Windows]**

`engine/rhi/CMakeLists.txt` 的 `me_rhi` 源追加 `src/DescriptorHeap.cpp`、`src/CommandContext.cpp`。
Run: `cmake --build build --config Debug --target me_rhi`
Expected: `me_rhi` 编译成功。

- [ ] **Step 6: 提交**

```bash
git add engine/rhi
git commit -m "feat(rhi): CommandContext(每帧录制)+ DescriptorHeap(RTV/SRV 分配)"
```

---

### Task 7: GpuBuffer(上传堆缓冲:顶点/索引)

最简单的资源:用上传堆(UPLOAD)创建并 memcpy 数据。M1 学习优先,不引入默认堆+拷贝队列(标注为后续性能审查点)。

**Files:**
- Create: `engine/rhi/include/me/rhi/GpuBuffer.h`, `engine/rhi/src/GpuBuffer.cpp`
- Modify: `engine/rhi/CMakeLists.txt`

**Interfaces:**
- Consumes: `GpuDevice`(Task 5)。
- Produces:
  - `class me::rhi::GpuBuffer`:
    - `static std::unique_ptr<GpuBuffer> CreateUpload(ID3D12Device* device, const void* data, size_t sizeBytes);`
    - `D3D12_GPU_VIRTUAL_ADDRESS Gpu() const;`
    - `size_t Size() const;`
    - `ID3D12Resource* Resource() const;`

- [ ] **Step 1: 写 `GpuBuffer.h`**

```cpp
#pragma once

#include <cstddef>
#include <memory>
#include <d3d12.h>

#include "me/rhi/D3DCommon.h"

namespace me::rhi {

/**
 * @brief 上传堆缓冲:CPU 可写、GPU 可读。M1 用于顶点/索引缓冲。
 *
 * 性能审查点:上传堆位于系统内存,GPU 每次读取需过 PCIe。生产应改用默认堆 +
 * 一次性拷贝。M1 以可读性优先,先这样。
 */
class GpuBuffer {
public:
    static std::unique_ptr<GpuBuffer> CreateUpload(ID3D12Device* device,
                                                   const void* data,
                                                   size_t sizeBytes);

    D3D12_GPU_VIRTUAL_ADDRESS Gpu() const { return m_resource->GetGPUVirtualAddress(); }
    size_t Size() const { return m_size; }
    ID3D12Resource* Resource() const { return m_resource.Get(); }

private:
    GpuBuffer() = default;
    ComPtr<ID3D12Resource> m_resource;
    size_t m_size = 0;
};

} // namespace me::rhi
```

- [ ] **Step 2: 写 `GpuBuffer.cpp`**

```cpp
#include "me/rhi/GpuBuffer.h"

#include <cstring>

namespace me::rhi {

std::unique_ptr<GpuBuffer> GpuBuffer::CreateUpload(ID3D12Device* device,
                                                   const void* data,
                                                   size_t sizeBytes) {
    auto self = std::unique_ptr<GpuBuffer>(new GpuBuffer());
    self->m_size = sizeBytes;

    D3D12_HEAP_PROPERTIES heap = {};
    heap.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = sizeBytes;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    if (FAILED(device->CreateCommittedResource(
            &heap, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&self->m_resource)))) {
        return nullptr;
    }

    // 映射并拷入数据(上传堆 CPU 可见)。
    void* mapped = nullptr;
    D3D12_RANGE noRead = {0, 0}; // CPU 不读
    if (FAILED(self->m_resource->Map(0, &noRead, &mapped))) {
        return nullptr;
    }
    std::memcpy(mapped, data, sizeBytes);
    self->m_resource->Unmap(0, nullptr);
    return self;
}

} // namespace me::rhi
```

- [ ] **Step 3: 加入源列表并编译验证 [Windows]**

`me_rhi` 源追加 `src/GpuBuffer.cpp`。
Run: `cmake --build build --config Debug --target me_rhi`
Expected: 编译成功。

- [ ] **Step 4: 提交**

```bash
git add engine/rhi
git commit -m "feat(rhi): GpuBuffer(上传堆,顶点/索引缓冲)"
```

---

### Task 8: GpuTexture + 像素回读 + 上传/回读往返测试

创建默认堆纹理、经上传堆同步上传像素、转到着色器可读状态并建 SRV;再写一个测试用的"纹理→CPU 像素"回读器。WARP 测试断言"上传什么、读回什么",证明纹理路径正确。

**Files:**
- Create: `engine/rhi/include/me/rhi/GpuTexture.h`, `engine/rhi/src/GpuTexture.cpp`
- Create: `engine/rhi/include/me/rhi/Readback.h`, `engine/rhi/src/Readback.cpp`
- Create: `tests/gpu/test_texture_roundtrip.cpp`
- Modify: `engine/rhi/CMakeLists.txt`

**Interfaces:**
- Consumes: `GpuDevice`、`Fence`(Task 5),`Descriptor`(Task 6)。**接收裸 RGBA8 像素指针,不依赖 me_assets**(保持 RHI→Assets 无依赖)。
- Produces:
  - `class me::rhi::GpuTexture`:
    - `static std::unique_ptr<GpuTexture> Create(ID3D12Device* device, ID3D12CommandQueue* queue, Fence& fence, uint32_t width, uint32_t height, const uint8_t* rgba8, const Descriptor& srv);`
    - `ID3D12Resource* Resource() const;`,`uint32_t Width() const;`,`uint32_t Height() const;`
    - `D3D12_GPU_DESCRIPTOR_HANDLE SrvGpu() const;`
  - `constexpr DXGI_FORMAT me::rhi::kSpriteTextureFormat = DXGI_FORMAT_R8G8B8A8_UNORM;`
  - `std::vector<uint8_t> me::rhi::ReadbackRgba8(ID3D12Device* device, ID3D12CommandQueue* queue, Fence& fence, ID3D12Resource* tex, uint32_t width, uint32_t height, D3D12_RESOURCE_STATES beforeState);` // 返回紧凑 RGBA8(无行填充)

- [ ] **Step 1: 写失败测试 `tests/gpu/test_texture_roundtrip.cpp`**

```cpp
#include <doctest/doctest.h>
#include <cstdint>
#include <vector>

#include "me/rhi/GpuDevice.h"
#include "me/rhi/Fence.h"
#include "me/rhi/DescriptorHeap.h"
#include "me/rhi/GpuTexture.h"
#include "me/rhi/Readback.h"

using namespace me::rhi;

TEST_CASE("2x2 纹理上传后逐像素回读一致") {
    auto device = GpuDevice::Create(true);
    REQUIRE(device != nullptr);
    auto fence = Fence::Create(device->Device());
    REQUIRE(fence != nullptr);
    auto srvHeap = DescriptorHeap::Create(
        device->Device(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 4, true);
    REQUIRE(srvHeap != nullptr);

    // 4 个不同颜色的 texel(RGBA8),行优先:左上、右上、左下、右下。
    const std::vector<uint8_t> pixels = {
        255, 0,   0,   255,   0,   255, 0,   255, // 红, 绿
        0,   0,   255, 255,   255, 255, 0,   255, // 蓝, 黄
    };
    auto srv = srvHeap->Allocate();
    auto tex = GpuTexture::Create(device->Device(), device->Queue(), *fence,
                                  2, 2, pixels.data(), srv);
    REQUIRE(tex != nullptr);

    auto out = ReadbackRgba8(device->Device(), device->Queue(), *fence,
                             tex->Resource(), 2, 2,
                             D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    REQUIRE(out.size() == pixels.size());
    for (size_t i = 0; i < pixels.size(); ++i) {
        CHECK(out[i] == pixels[i]);
    }
}
```

- [ ] **Step 2: 写 `GpuTexture.h`**

```cpp
#pragma once

#include <cstdint>
#include <memory>
#include <d3d12.h>

#include "me/rhi/D3DCommon.h"
#include "me/rhi/DescriptorHeap.h"

namespace me::rhi {

class Fence;

/// 精灵纹理像素格式(具名常量)。
constexpr DXGI_FORMAT kSpriteTextureFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

/**
 * @brief 默认堆 2D 纹理:同步上传 RGBA8 像素、转到 PIXEL_SHADER_RESOURCE、建 SRV。
 *
 * 接收裸像素指针(不依赖 Assets 层);上传在 Create 内同步完成(Fence 刷队列)。
 */
class GpuTexture {
public:
    static std::unique_ptr<GpuTexture> Create(ID3D12Device* device,
                                              ID3D12CommandQueue* queue,
                                              Fence& fence,
                                              uint32_t width, uint32_t height,
                                              const uint8_t* rgba8,
                                              const Descriptor& srv);

    ID3D12Resource* Resource() const { return m_resource.Get(); }
    uint32_t Width() const { return m_width; }
    uint32_t Height() const { return m_height; }
    D3D12_GPU_DESCRIPTOR_HANDLE SrvGpu() const { return m_srvGpu; }

private:
    GpuTexture() = default;
    ComPtr<ID3D12Resource> m_resource;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    D3D12_GPU_DESCRIPTOR_HANDLE m_srvGpu{};
};

} // namespace me::rhi
```

- [ ] **Step 3: 写 `GpuTexture.cpp`**

```cpp
#include "me/rhi/GpuTexture.h"

#include <cstring>
#include "me/rhi/Fence.h"

namespace me::rhi {

namespace {
constexpr uint32_t kBytesPerPixel = 4; // RGBA8
} // namespace

std::unique_ptr<GpuTexture> GpuTexture::Create(ID3D12Device* device,
                                               ID3D12CommandQueue* queue,
                                               Fence& fence,
                                               uint32_t width, uint32_t height,
                                               const uint8_t* rgba8,
                                               const Descriptor& srv) {
    auto self = std::unique_ptr<GpuTexture>(new GpuTexture());
    self->m_width = width;
    self->m_height = height;

    // 1) 默认堆纹理(COPY_DEST 起始)。
    D3D12_HEAP_PROPERTIES defaultHeap = {};
    defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = kSpriteTextureFormat;
    texDesc.SampleDesc.Count = 1;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

    if (FAILED(device->CreateCommittedResource(
            &defaultHeap, D3D12_HEAP_FLAG_NONE, &texDesc,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
            IID_PPV_ARGS(&self->m_resource)))) {
        return nullptr;
    }

    // 2) 计算可拷贝足迹,建上传缓冲。
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
    UINT numRows = 0;
    UINT64 rowSizeBytes = 0, totalBytes = 0;
    device->GetCopyableFootprints(&texDesc, 0, 1, 0, &footprint, &numRows,
                                  &rowSizeBytes, &totalBytes);

    D3D12_HEAP_PROPERTIES uploadHeap = {};
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC bufDesc = {};
    bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufDesc.Width = totalBytes;
    bufDesc.Height = 1;
    bufDesc.DepthOrArraySize = 1;
    bufDesc.MipLevels = 1;
    bufDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufDesc.SampleDesc.Count = 1;
    bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    ComPtr<ID3D12Resource> upload;
    if (FAILED(device->CreateCommittedResource(
            &uploadHeap, D3D12_HEAP_FLAG_NONE, &bufDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&upload)))) {
        return nullptr;
    }

    // 3) 按行(对齐到 footprint.Footprint.RowPitch)拷入像素。
    uint8_t* mapped = nullptr;
    D3D12_RANGE noRead = {0, 0};
    if (FAILED(upload->Map(0, &noRead, reinterpret_cast<void**>(&mapped)))) {
        return nullptr;
    }
    const uint32_t srcRowBytes = width * kBytesPerPixel;
    for (uint32_t row = 0; row < height; ++row) {
        std::memcpy(mapped + footprint.Offset + row * footprint.Footprint.RowPitch,
                    rgba8 + row * srcRowBytes, srcRowBytes);
    }
    upload->Unmap(0, nullptr);

    // 4) 录制拷贝 + 状态转换(自建一次性命令列表)。
    ComPtr<ID3D12CommandAllocator> alloc;
    ComPtr<ID3D12GraphicsCommandList> list;
    if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                              IID_PPV_ARGS(&alloc))) ||
        FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                         alloc.Get(), nullptr, IID_PPV_ARGS(&list)))) {
        return nullptr;
    }

    D3D12_TEXTURE_COPY_LOCATION dst = {};
    dst.pResource = self->m_resource.Get();
    dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst.SubresourceIndex = 0;
    D3D12_TEXTURE_COPY_LOCATION src = {};
    src.pResource = upload.Get();
    src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    src.PlacedFootprint = footprint;
    list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = self->m_resource.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    list->ResourceBarrier(1, &barrier);

    ME_HR_CHECK(list->Close());
    ID3D12CommandList* lists[] = {list.Get()};
    queue->ExecuteCommandLists(1, lists);
    fence.Flush(queue); // 同步等待上传完成,之后 upload/alloc 可安全释放

    // 5) 建 SRV。
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = kSpriteTextureFormat;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = 1;
    device->CreateShaderResourceView(self->m_resource.Get(), &srvDesc, srv.cpu);
    self->m_srvGpu = srv.gpu;
    return self;
}

} // namespace me::rhi
```

- [ ] **Step 4: 写 `Readback.h`**

```cpp
#pragma once

#include <cstdint>
#include <vector>
#include <d3d12.h>

namespace me::rhi {

class Fence;

/**
 * @brief 把 RGBA8 纹理同步回读为紧凑(无行填充)CPU 像素。仅供测试/调试。
 *
 * beforeState 是纹理当前所处状态(回读前转成 COPY_SOURCE,回读后转回)。
 */
std::vector<uint8_t> ReadbackRgba8(ID3D12Device* device,
                                   ID3D12CommandQueue* queue,
                                   Fence& fence,
                                   ID3D12Resource* tex,
                                   uint32_t width, uint32_t height,
                                   D3D12_RESOURCE_STATES beforeState);

} // namespace me::rhi
```

- [ ] **Step 5: 写 `Readback.cpp`**

```cpp
#include "me/rhi/Readback.h"

#include <cstring>
#include "me/rhi/D3DCommon.h"
#include "me/rhi/Fence.h"

namespace me::rhi {

namespace {
constexpr uint32_t kBytesPerPixel = 4;
} // namespace

std::vector<uint8_t> ReadbackRgba8(ID3D12Device* device,
                                   ID3D12CommandQueue* queue,
                                   Fence& fence,
                                   ID3D12Resource* tex,
                                   uint32_t width, uint32_t height,
                                   D3D12_RESOURCE_STATES beforeState) {
    D3D12_RESOURCE_DESC texDesc = tex->GetDesc();
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
    UINT numRows = 0;
    UINT64 rowSizeBytes = 0, totalBytes = 0;
    device->GetCopyableFootprints(&texDesc, 0, 1, 0, &footprint, &numRows,
                                  &rowSizeBytes, &totalBytes);

    // 回读堆缓冲。
    D3D12_HEAP_PROPERTIES readbackHeap = {};
    readbackHeap.Type = D3D12_HEAP_TYPE_READBACK;
    D3D12_RESOURCE_DESC bufDesc = {};
    bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufDesc.Width = totalBytes;
    bufDesc.Height = 1;
    bufDesc.DepthOrArraySize = 1;
    bufDesc.MipLevels = 1;
    bufDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufDesc.SampleDesc.Count = 1;
    bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    ComPtr<ID3D12Resource> readback;
    ME_HR_CHECK(device->CreateCommittedResource(
        &readbackHeap, D3D12_HEAP_FLAG_NONE, &bufDesc,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readback)));

    ComPtr<ID3D12CommandAllocator> alloc;
    ComPtr<ID3D12GraphicsCommandList> list;
    ME_HR_CHECK(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                               IID_PPV_ARGS(&alloc)));
    ME_HR_CHECK(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                          alloc.Get(), nullptr, IID_PPV_ARGS(&list)));

    auto transition = [&](D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to) {
        D3D12_RESOURCE_BARRIER b = {};
        b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b.Transition.pResource = tex;
        b.Transition.StateBefore = from;
        b.Transition.StateAfter = to;
        b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        list->ResourceBarrier(1, &b);
    };

    transition(beforeState, D3D12_RESOURCE_STATE_COPY_SOURCE);
    D3D12_TEXTURE_COPY_LOCATION dst = {};
    dst.pResource = readback.Get();
    dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    dst.PlacedFootprint = footprint;
    D3D12_TEXTURE_COPY_LOCATION src = {};
    src.pResource = tex;
    src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    src.SubresourceIndex = 0;
    list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    transition(D3D12_RESOURCE_STATE_COPY_SOURCE, beforeState);

    ME_HR_CHECK(list->Close());
    ID3D12CommandList* lists[] = {list.Get()};
    queue->ExecuteCommandLists(1, lists);
    fence.Flush(queue);

    // 去掉行填充,拷成紧凑 RGBA8。
    std::vector<uint8_t> out(static_cast<size_t>(width) * height * kBytesPerPixel);
    uint8_t* mapped = nullptr;
    D3D12_RANGE readRange = {0, static_cast<SIZE_T>(totalBytes)};
    ME_HR_CHECK(readback->Map(0, &readRange, reinterpret_cast<void**>(&mapped)));
    const uint32_t dstRowBytes = width * kBytesPerPixel;
    for (uint32_t row = 0; row < height; ++row) {
        std::memcpy(out.data() + row * dstRowBytes,
                    mapped + footprint.Offset + row * footprint.Footprint.RowPitch,
                    dstRowBytes);
    }
    D3D12_RANGE noWrite = {0, 0};
    readback->Unmap(0, &noWrite);
    return out;
}

} // namespace me::rhi
```

- [ ] **Step 6: 加入源/测试列表**

`me_rhi` 源追加 `src/GpuTexture.cpp`、`src/Readback.cpp`。
`me_gpu_tests` 源追加 `${CMAKE_SOURCE_DIR}/tests/gpu/test_texture_roundtrip.cpp`。

- [ ] **Step 7: 运行 GPU 测试确认通过 [Windows]**

Run:
```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DME_BUILD_TESTS=ON -DME_BUILD_GPU_TESTS=ON
cmake --build build --config Debug
ctest --test-dir build -C Debug -R me_gpu_tests --output-on-failure
```
Expected: 含纹理往返用例的 `me_gpu_tests` 全 PASS。

- [ ] **Step 8: 提交**

```bash
git add engine/rhi tests/gpu/test_texture_roundtrip.cpp
git commit -m "feat(rhi): GpuTexture(同步上传+SRV)+ 像素回读 + 往返测试"
```

---

### Task 9: Shader 运行时编译(FXC)+ sprite.hlsl

用 `D3DCompileFromFile` 在运行时编译 HLSL(你选的 FXC 路线),并写出精灵着色器。MVP 矩阵以 `row_major` 接收,匹配 M0 行向量约定。

**Files:**
- Create: `engine/rhi/include/me/rhi/Shader.h`, `engine/rhi/src/Shader.cpp`
- Create: `assets/shaders/sprite.hlsl`
- Modify: `engine/rhi/CMakeLists.txt`

**Interfaces (Produces):**
- `me::rhi::ComPtr<ID3DBlob> me::rhi::CompileHlsl(const std::wstring& path, const char* entry, const char* target);` // 失败返回 nullptr,错误打到 Log
- 着色器约定:顶点入口 `VSMain`,像素入口 `PSMain`;根签名 = 16 个 32-bit 根常量(MVP,b0)+ SRV 描述符表(t0)+ 静态采样器(s0)。

- [ ] **Step 1: 写 `assets/shaders/sprite.hlsl`**

```hlsl
// 精灵着色器:把单位四边形经 MVP 变换后采样纹理。
// MVP 用 row_major 接收,匹配引擎的行主序存储 + 行向量约定(v' = v * M)。

cbuffer SpriteConstants : register(b0)
{
    row_major float4x4 uMVP;
};

Texture2D    gTexture : register(t0);
SamplerState gSampler : register(s0);

struct VSInput
{
    float2 position : POSITION; // 局部空间 ±0.5
    float2 uv       : TEXCOORD0;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD0;
};

PSInput VSMain(VSInput input)
{
    PSInput o;
    // 行向量:把局部点当作行向量左乘 MVP。
    o.position = mul(float4(input.position, 0.0f, 1.0f), uMVP);
    o.uv = input.uv;
    return o;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return gTexture.Sample(gSampler, input.uv);
}
```

- [ ] **Step 2: 写 `Shader.h`**

```cpp
#pragma once

#include <string>
#include <d3d12.h>
#include <d3dcommon.h> // ID3DBlob

#include "me/rhi/D3DCommon.h"

namespace me::rhi {

/**
 * @brief 运行时用 FXC(D3DCompileFromFile)编译一个 HLSL 入口为字节码。
 *
 * @param path   .hlsl 绝对路径(由 ME_ASSET_DIR 拼出)。
 * @param entry  入口函数名(如 "VSMain")。
 * @param target 着色器模型(如 "vs_5_1" / "ps_5_1")。
 * @return 字节码 Blob;失败返回 nullptr 并把编译错误写入 Log。
 */
ComPtr<ID3DBlob> CompileHlsl(const std::wstring& path, const char* entry,
                             const char* target);

} // namespace me::rhi
```

- [ ] **Step 3: 写 `Shader.cpp`**

```cpp
#include "me/rhi/Shader.h"

#include <d3dcompiler.h>
#include "me/core/Log.h"

namespace me::rhi {

ComPtr<ID3DBlob> CompileHlsl(const std::wstring& path, const char* entry,
                             const char* target) {
    UINT flags = 0;
#if !defined(NDEBUG)
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
    ComPtr<ID3DBlob> code;
    ComPtr<ID3DBlob> errors;
    const HRESULT hr = D3DCompileFromFile(
        path.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
        entry, target, flags, 0, &code, &errors);
    if (FAILED(hr)) {
        if (errors) {
            ME_LOG_ERROR(std::string("着色器编译失败: ") +
                         static_cast<const char*>(errors->GetBufferPointer()));
        } else {
            ME_LOG_ERROR("着色器编译失败(文件无法打开?)");
        }
        return nullptr;
    }
    return code;
}

} // namespace me::rhi
```

- [ ] **Step 4: 加入源列表并编译 [Windows]**

`me_rhi` 源追加 `src/Shader.cpp`(已链接 `d3dcompiler`)。
Run: `cmake --build build --config Debug --target me_rhi`
Expected: 编译成功。

- [ ] **Step 5: 提交**

```bash
git add engine/rhi assets/shaders/sprite.hlsl
git commit -m "feat(rhi): HLSL 运行时编译(FXC)+ sprite.hlsl(row_major MVP)"
```

---

### Task 10: Assets 图片解码 `me_assets`(stb_image,跨平台)

把 PNG/JPG 解码成 RGBA8 `ImageData`。stb_image 可移植,**整个模块可在 WSL 单测**。

**Files:**
- Modify: `third_party/CMakeLists.txt`(FetchContent stb)
- Create: `engine/assets/include/me/assets/ImageData.h`
- Create: `engine/assets/src/ImageLoader.cpp`, `engine/assets/src/stb_image_impl.cpp`
- Create: `engine/assets/CMakeLists.txt`, `engine/assets/README.md`
- Test: `tests/assets/test_image.cpp`
- Modify: `CMakeLists.txt`(add_subdirectory)、`tests/CMakeLists.txt`

**Interfaces (Produces):**
- `struct me::assets::ImageData { int width=0; int height=0; std::vector<std::uint8_t> pixels; bool IsValid() const; }` // pixels 为紧凑 RGBA8
- `std::optional<ImageData> me::assets::LoadImageRGBA8(const std::string& path);`
- `std::optional<ImageData> me::assets::DecodeImageRGBA8(const std::uint8_t* bytes, std::size_t size);`

- [ ] **Step 1: 在 `third_party/CMakeLists.txt` 末尾追加 stb**

```cmake
# stb:单头库集合。仅用 stb_image(贴图解码)。
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(stb)

add_library(stb_image INTERFACE)
target_include_directories(stb_image INTERFACE ${stb_SOURCE_DIR})
```

> stb 无版本 tag,用 `master`。若要可复现构建,后续可把某次 commit 哈希钉入 `GIT_TAG`。

- [ ] **Step 2: 写失败测试 `tests/assets/test_image.cpp`**

```cpp
#include <doctest/doctest.h>
#include <cstdint>
#include <vector>

#include "me/assets/ImageData.h"

using me::assets::DecodeImageRGBA8;

// 一个最小的 2x2 PNG(红/绿/蓝/白),以字节数组内嵌,避免依赖磁盘文件。
// 生成方式见计划注释;此处为合法 PNG 字节流。
static const std::vector<std::uint8_t> kPng2x2 = {
    0x89,0x50,0x4E,0x47,0x0D,0x0A,0x1A,0x0A, 0x00,0x00,0x00,0x0D,0x49,0x48,0x44,0x52,
    0x00,0x00,0x00,0x02,0x00,0x00,0x00,0x02, 0x08,0x02,0x00,0x00,0x00,0xFD,0xD4,0x9A,
    0x73,0x00,0x00,0x00,0x19,0x49,0x44,0x41, 0x54,0x08,0x99,0x63,0xF8,0xCF,0xC0,0xF0,
    0x9F,0x81,0x81,0x01,0x88,0xC4,0x7F,0x0C, 0x0C,0x0C,0x40,0x00,0x00,0x12,0x9D,0x02,
    0x7E,0xE3,0x0E,0xF1,0xCE,0x00,0x00,0x00, 0x00,0x49,0x45,0x4E,0x44,0xAE,0x42,0x60,
    0x82,
};

TEST_CASE("DecodeImageRGBA8 解出 2x2 RGBA") {
    auto img = DecodeImageRGBA8(kPng2x2.data(), kPng2x2.size());
    REQUIRE(img.has_value());
    CHECK(img->width == 2);
    CHECK(img->height == 2);
    CHECK(img->pixels.size() == static_cast<size_t>(2 * 2 * 4));
    CHECK(img->IsValid());
}

TEST_CASE("非法字节返回 nullopt") {
    const std::vector<std::uint8_t> garbage = {0, 1, 2, 3, 4};
    auto img = DecodeImageRGBA8(garbage.data(), garbage.size());
    CHECK_FALSE(img.has_value());
}
```

> **执行者注意**:`kPng2x2` 须为真实合法 PNG。实现本任务时,用以下命令在仓库根生成并把字节填入数组(Python 自带 zlib,可生成最小 PNG):
> ```bash
> python3 - <<'PY'
> import struct, zlib
> def chunk(t,d): 
>     c=t+d; return struct.pack(">I",len(d))+c+struct.pack(">I",zlib.crc32(c)&0xffffffff)
> w=h=2
> raw=b''.join(b'\x00'+bytes([r*120%256,g*120%256,(r+g)*80%256]) for r in range(h) for g in range(w))
> png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack(">IIBBBBB",w,h,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(raw))+chunk(b'IEND',b'')
> print(', '.join('0x%02X'%b for b in png))
> PY
> ```
> 用输出替换数组内容(尺寸保持 2x2,断言不变)。

- [ ] **Step 3: 运行测试确认失败 [WSL]**

Run: `cmake -S . -B build-wsl -DME_BUILD_TESTS=ON -DME_BUILD_GPU_TESTS=OFF && cmake --build build-wsl -j`
Expected: 编译失败(`me/assets/ImageData.h` 不存在)。

- [ ] **Step 4: 写 `ImageData.h`**

```cpp
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace me::assets {

/** @brief 解码后的位图:紧凑 RGBA8(每像素 4 字节,无行填充)。 */
struct ImageData {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> pixels; // 大小 = width*height*4

    bool IsValid() const {
        return width > 0 && height > 0 &&
               pixels.size() == static_cast<std::size_t>(width) * height * 4;
    }
};

/** @brief 从磁盘加载图片并转 RGBA8;失败返回 std::nullopt(不抛异常)。 */
std::optional<ImageData> LoadImageRGBA8(const std::string& path);

/** @brief 从内存字节解码图片为 RGBA8;失败返回 std::nullopt。 */
std::optional<ImageData> DecodeImageRGBA8(const std::uint8_t* bytes,
                                          std::size_t size);

} // namespace me::assets
```

- [ ] **Step 5: 写 `stb_image_impl.cpp`(单独 TU 定义实现宏)**

```cpp
// stb_image 的实现仅在此唯一 TU 展开,避免重复符号。
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO // 只用内存解码接口,文件 IO 走引擎 FileSystem
#include "stb_image.h"
```

- [ ] **Step 6: 写 `ImageLoader.cpp`**

```cpp
#include "me/assets/ImageData.h"

#include "stb_image.h"
#include "me/platform/FileSystem.h"

namespace me::assets {

namespace {
constexpr int kRequiredChannels = 4; // 强制 RGBA
} // namespace

std::optional<ImageData> DecodeImageRGBA8(const std::uint8_t* bytes,
                                          std::size_t size) {
    int w = 0, h = 0, channelsInFile = 0;
    stbi_uc* decoded = stbi_load_from_memory(
        bytes, static_cast<int>(size), &w, &h, &channelsInFile, kRequiredChannels);
    if (decoded == nullptr) {
        return std::nullopt; // 解码失败:非图片/损坏
    }
    ImageData img;
    img.width = w;
    img.height = h;
    img.pixels.assign(decoded,
                      decoded + static_cast<std::size_t>(w) * h * kRequiredChannels);
    stbi_image_free(decoded);
    return img;
}

std::optional<ImageData> LoadImageRGBA8(const std::string& path) {
    auto bytes = me::platform::ReadBinaryFile(path);
    if (!bytes.has_value()) {
        return std::nullopt; // 文件不存在/不可读
    }
    return DecodeImageRGBA8(bytes->data(), bytes->size());
}

} // namespace me::assets
```

- [ ] **Step 7: 写 `engine/assets/CMakeLists.txt` 与 `README.md`**

```cmake
add_library(me_assets STATIC
    src/ImageLoader.cpp
    src/stb_image_impl.cpp
)
target_include_directories(me_assets PUBLIC include)
target_compile_features(me_assets PUBLIC cxx_std_17)
# 单向依赖:assets → core, platform;stb_image 仅私有实现细节。
target_link_libraries(me_assets PUBLIC me_core me_platform PRIVATE stb_image)
```

`engine/assets/README.md`:
```markdown
# engine/assets(me_assets)

资源解码层。M1 仅含图片解码(stb_image)→ 紧凑 RGBA8 `ImageData`。
跨平台,可在 WSL 单测。依赖:Core, Platform(单向)。
后续里程碑扩展 AssetManager/句柄/图集/瓦片集/内容 JSON。
```

- [ ] **Step 8: 接入构建并运行测试 [WSL]**

根 `CMakeLists.txt` 在 `add_subdirectory(engine/rhi)` 后加 `add_subdirectory(engine/assets)`。
`tests/CMakeLists.txt` 的 `me_tests` 源追加 `assets/test_image.cpp`,链接库追加 `me_assets`。
Run: `cmake -S . -B build-wsl -DME_BUILD_TESTS=ON -DME_BUILD_GPU_TESTS=OFF && cmake --build build-wsl -j && ctest --test-dir build-wsl --output-on-failure`
Expected: PASS,图片解码用例绿。

- [ ] **Step 9: 提交**

```bash
git add third_party/CMakeLists.txt engine/assets tests/assets/test_image.cpp tests/CMakeLists.txt CMakeLists.txt
git commit -m "feat(assets): me_assets 图片解码(stb_image)→ RGBA8 + 单测"
```

---

### Task 11: SwapChain(交换链 + 后台缓冲 RTV)

把 DXGI 翻转交换链 + 每个后台缓冲的 RTV 封装好,供 sandbox 每帧清屏/呈现。需要 HWND,只能 sandbox 目视验证(无窗口的回读测试不经交换链,故本任务无 GPU 单测)。

**Files:**
- Create: `engine/rhi/include/me/rhi/SwapChain.h`, `engine/rhi/src/SwapChain.cpp`
- Modify: `engine/rhi/CMakeLists.txt`

**Interfaces:**
- Consumes: `GpuDevice`(Task 5),`DescriptorHeap`(Task 6)。
- Produces:
  - `class me::rhi::SwapChain`:
    - `static std::unique_ptr<SwapChain> Create(GpuDevice& device, void* hwnd, uint32_t width, uint32_t height);`
    - `uint32_t BackBufferIndex() const;`
    - `ID3D12Resource* CurrentBackBuffer() const;`
    - `D3D12_CPU_DESCRIPTOR_HANDLE CurrentRtv() const;`
    - `void Present();`
    - `uint32_t Width() const;` / `uint32_t Height() const;`

- [ ] **Step 1: 写 `SwapChain.h`**

```cpp
#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <d3d12.h>
#include <dxgi1_6.h>

#include "me/rhi/D3DCommon.h"
#include "me/rhi/FrameRing.h"
#include "me/rhi/DescriptorHeap.h"

namespace me::rhi {

class GpuDevice;

/**
 * @brief DXGI 翻转模型交换链 + 每后台缓冲一个 RTV。
 *
 * 翻转模型(FLIP_DISCARD)是 DX12 的现代呈现路径。kFrameCount 个后台缓冲与
 * CommandContext 的帧分配器一一对应。
 */
class SwapChain {
public:
    static std::unique_ptr<SwapChain> Create(GpuDevice& device, void* hwnd,
                                             uint32_t width, uint32_t height);

    uint32_t BackBufferIndex() const { return m_swapChain->GetCurrentBackBufferIndex(); }
    ID3D12Resource* CurrentBackBuffer() const {
        return m_backBuffers[BackBufferIndex()].Get();
    }
    D3D12_CPU_DESCRIPTOR_HANDLE CurrentRtv() const { return m_rtvs[BackBufferIndex()]; }
    void Present() { m_swapChain->Present(1, 0); } // 垂直同步

    uint32_t Width() const { return m_width; }
    uint32_t Height() const { return m_height; }

private:
    SwapChain() = default;
    ComPtr<IDXGISwapChain3> m_swapChain;
    std::unique_ptr<DescriptorHeap> m_rtvHeap;
    std::array<ComPtr<ID3D12Resource>, kFrameCount> m_backBuffers;
    std::array<D3D12_CPU_DESCRIPTOR_HANDLE, kFrameCount> m_rtvs{};
    uint32_t m_width = 0;
    uint32_t m_height = 0;
};

} // namespace me::rhi
```

- [ ] **Step 2: 写 `SwapChain.cpp`**

```cpp
#include "me/rhi/SwapChain.h"

#include <windows.h>
#include "me/rhi/GpuDevice.h"

namespace me::rhi {

std::unique_ptr<SwapChain> SwapChain::Create(GpuDevice& device, void* hwnd,
                                             uint32_t width, uint32_t height) {
    auto self = std::unique_ptr<SwapChain>(new SwapChain());
    self->m_width = width;
    self->m_height = height;

    DXGI_SWAP_CHAIN_DESC1 desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = kFrameCount;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    ComPtr<IDXGISwapChain1> sc1;
    if (FAILED(device.Factory()->CreateSwapChainForHwnd(
            device.Queue(), static_cast<HWND>(hwnd), &desc, nullptr, nullptr, &sc1))) {
        return nullptr;
    }
    if (FAILED(sc1.As(&self->m_swapChain))) {
        return nullptr;
    }

    self->m_rtvHeap = DescriptorHeap::Create(
        device.Device(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, kFrameCount, false);
    if (self->m_rtvHeap == nullptr) {
        return nullptr;
    }

    for (uint32_t i = 0; i < kFrameCount; ++i) {
        if (FAILED(self->m_swapChain->GetBuffer(i, IID_PPV_ARGS(&self->m_backBuffers[i])))) {
            return nullptr;
        }
        Descriptor d = self->m_rtvHeap->Allocate();
        device.Device()->CreateRenderTargetView(
            self->m_backBuffers[i].Get(), nullptr, d.cpu);
        self->m_rtvs[i] = d.cpu;
    }
    return self;
}

} // namespace me::rhi
```

- [ ] **Step 3: 加入源列表并编译 [Windows]**

`me_rhi` 源追加 `src/SwapChain.cpp`。
Run: `cmake --build build --config Debug --target me_rhi`
Expected: 编译成功。

- [ ] **Step 4: 提交**

```bash
git add engine/rhi
git commit -m "feat(rhi): SwapChain(翻转模型交换链 + 后台缓冲 RTV)"
```

---

### Task 12: SpriteRenderer(me_renderer)+ 带纹理精灵渲染回读测试

把根签名 + PSO + 单位四边形 VB/IB 封装成 `SpriteRenderer::Draw`,**sandbox 与 GPU 测试共用**。WARP 测试:渲染到离屏 RT、回读像素,断言"中心=贴图色、四角=清屏色",这是 M1 "带纹理精灵"的自动化把关。

**Files:**
- Create: `engine/renderer/include/me/render/SpriteRenderer.h`, `engine/renderer/src/SpriteRenderer.cpp`
- Create: `engine/renderer/CMakeLists.txt`, `engine/renderer/README.md`
- Create: `tests/gpu/test_sprite_render.cpp`
- Modify: `CMakeLists.txt`(add_subdirectory)、`engine/rhi/CMakeLists.txt`(`me_gpu_tests` 链接 `me_renderer`、加测试源)

**Interfaces:**
- Consumes: `GpuDevice`、`GpuBuffer`、`GpuTexture`、`Shader`、`Descriptor`(RHI),`Matrix4x4`(Core),`UnitQuadVertices/Indices`/`SpriteVertex`(me_rhi_cpu)。
- Produces:
  - `class me::render::SpriteRenderer`:
    - `static std::unique_ptr<SpriteRenderer> Create(me::rhi::GpuDevice& device);` // 编译着色器/建根签名+PSO/上传四边形,失败 nullptr
    - `void Draw(ID3D12GraphicsCommandList* cmd, const me::rhi::GpuTexture& tex, const me::Matrix4x4& mvp);` // 假定调用方已设好 RT/视口/裁剪并已清屏

- [ ] **Step 1: 写失败测试 `tests/gpu/test_sprite_render.cpp`**

```cpp
#include <doctest/doctest.h>
#include <cstdint>
#include <vector>

#include "me/rhi/GpuDevice.h"
#include "me/rhi/Fence.h"
#include "me/rhi/DescriptorHeap.h"
#include "me/rhi/CommandContext.h"
#include "me/rhi/GpuTexture.h"
#include "me/rhi/Readback.h"
#include "me/core/Matrix4x4.h"
#include "me/core/Vector2.h"
#include "me/render/SpriteRenderer.h"

using namespace me::rhi;
using me::render::SpriteRenderer;

namespace {
constexpr uint32_t kRt = 8; // 8x8 离屏目标

// 创建一个 8x8、RENDER_TARGET 起始状态的离屏纹理 + RTV。
ComPtr<ID3D12Resource> MakeRenderTarget(ID3D12Device* device,
                                        D3D12_CPU_DESCRIPTOR_HANDLE rtv) {
    D3D12_HEAP_PROPERTIES heap = {};
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = kRt; desc.Height = kRt;
    desc.DepthOrArraySize = 1; desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    D3D12_CLEAR_VALUE clear = {};
    clear.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    ComPtr<ID3D12Resource> rt;
    device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
        D3D12_RESOURCE_STATE_RENDER_TARGET, &clear, IID_PPV_ARGS(&rt));
    device->CreateRenderTargetView(rt.Get(), nullptr, rtv);
    return rt;
}
} // namespace

TEST_CASE("带纹理精灵:中心=贴图色,四角=清屏色") {
    auto device = GpuDevice::Create(true);
    REQUIRE(device != nullptr);
    auto fence = Fence::Create(device->Device());
    auto ctx = CommandContext::Create(device->Device());
    auto rtvHeap = DescriptorHeap::Create(device->Device(),
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1, false);
    auto srvHeap = DescriptorHeap::Create(device->Device(),
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1, true);
    REQUIRE(fence); REQUIRE(ctx); REQUIRE(rtvHeap); REQUIRE(srvHeap);

    auto rtvDesc = rtvHeap->Allocate();
    auto rt = MakeRenderTarget(device->Device(), rtvDesc.cpu);

    // 1x1 纯红贴图 → 任意采样都得红,回读判定稳定。
    const uint8_t red[4] = {255, 0, 0, 255};
    auto srv = srvHeap->Allocate();
    auto tex = GpuTexture::Create(device->Device(), device->Queue(), *fence,
                                  1, 1, red, srv);
    REQUIRE(tex != nullptr);

    auto renderer = SpriteRenderer::Create(*device);
    REQUIRE(renderer != nullptr);

    // 投影=单位,模型=缩放 0.5:四边形覆盖 NDC 中央 1/4,四角留清屏色。
    me::Matrix4x4 mvp = me::Matrix4x4::Scale(me::Vector2{0.5f, 0.5f});

    auto* cmd = ctx->Begin();
    D3D12_VIEWPORT vp = {0, 0, float(kRt), float(kRt), 0.0f, 1.0f};
    D3D12_RECT sc = {0, 0, LONG(kRt), LONG(kRt)};
    cmd->RSSetViewports(1, &vp);
    cmd->RSSetScissorRects(1, &sc);
    cmd->OMSetRenderTargets(1, &rtvDesc.cpu, FALSE, nullptr);
    const float blue[4] = {0.0f, 0.0f, 1.0f, 1.0f};
    cmd->ClearRenderTargetView(rtvDesc.cpu, blue, 0, nullptr);
    ID3D12DescriptorHeap* heaps[] = {srvHeap->Heap()};
    cmd->SetDescriptorHeaps(1, heaps);
    renderer->Draw(cmd, *tex, mvp);
    ctx->End();
    ctx->Execute(device->Queue());
    fence->Flush(device->Queue());

    auto px = ReadbackRgba8(device->Device(), device->Queue(), *fence,
                            rt.Get(), kRt, kRt, D3D12_RESOURCE_STATE_RENDER_TARGET);
    auto at = [&](uint32_t x, uint32_t y) { return &px[(y * kRt + x) * 4]; };

    // 中心像素 ≈ 红
    CHECK(at(kRt/2, kRt/2)[0] > 200);
    CHECK(at(kRt/2, kRt/2)[2] < 60);
    // 角像素 ≈ 蓝(清屏色)
    CHECK(at(0, 0)[2] > 200);
    CHECK(at(0, 0)[0] < 60);
}
```

- [ ] **Step 2: 写 `SpriteRenderer.h`**

```cpp
#pragma once

#include <memory>
#include <d3d12.h>

#include "me/rhi/D3DCommon.h"
#include "me/core/Matrix4x4.h"

namespace me::rhi { class GpuDevice; class GpuBuffer; class GpuTexture; }

namespace me::render {

/**
 * @brief 单精灵渲染器(M1):一个根签名 + PSO + 单位四边形 VB/IB。
 *
 * Draw 假定调用方已绑定 RT/视口/裁剪、已清屏、已 SetDescriptorHeaps(SRV 堆)。
 * MVP 经根常量传入。M2 将在此之上扩展为 SpriteBatch + 正交相机。
 */
class SpriteRenderer {
public:
    static std::unique_ptr<SpriteRenderer> Create(me::rhi::GpuDevice& device);
    ~SpriteRenderer();

    void Draw(ID3D12GraphicsCommandList* cmd, const me::rhi::GpuTexture& tex,
              const me::Matrix4x4& mvp);

private:
    SpriteRenderer() = default;
    me::rhi::ComPtr<ID3D12RootSignature> m_rootSig;
    me::rhi::ComPtr<ID3D12PipelineState> m_pso;
    std::unique_ptr<me::rhi::GpuBuffer> m_vb;
    std::unique_ptr<me::rhi::GpuBuffer> m_ib;
    D3D12_VERTEX_BUFFER_VIEW m_vbv{};
    D3D12_INDEX_BUFFER_VIEW m_ibv{};
};

} // namespace me::render
```

- [ ] **Step 3: 写 `SpriteRenderer.cpp`**

```cpp
#include "me/render/SpriteRenderer.h"

#include <string>
#include "me/rhi/GpuDevice.h"
#include "me/rhi/GpuBuffer.h"
#include "me/rhi/GpuTexture.h"
#include "me/rhi/Shader.h"
#include "me/rhi/QuadGeometry.h"

namespace me::render {

namespace {
using namespace me::rhi;

constexpr uint32_t kMvpConstantCount = 16; // 4x4 float
constexpr uint32_t kRootParamMvp = 0;
constexpr uint32_t kRootParamTexture = 1;

std::wstring ShaderPath() {
    // ME_ASSET_DIR 由 CMake 注入(仓库 assets 绝对路径)。
    const std::string p = std::string(ME_ASSET_DIR) + "/shaders/sprite.hlsl";
    return std::wstring(p.begin(), p.end());
}
} // namespace

std::unique_ptr<SpriteRenderer> SpriteRenderer::Create(GpuDevice& device) {
    auto self = std::unique_ptr<SpriteRenderer>(new SpriteRenderer());
    ID3D12Device* dev = device.Device();

    // 1) 着色器。
    ComPtr<ID3DBlob> vs = CompileHlsl(ShaderPath(), "VSMain", "vs_5_1");
    ComPtr<ID3DBlob> ps = CompileHlsl(ShaderPath(), "PSMain", "ps_5_1");
    if (!vs || !ps) return nullptr;

    // 2) 根签名:b0 = 16 个根常量(MVP);t0 = SRV 描述符表;s0 = 静态采样器。
    D3D12_DESCRIPTOR_RANGE srvRange = {};
    srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRange.NumDescriptors = 1;
    srvRange.BaseShaderRegister = 0; // t0

    D3D12_ROOT_PARAMETER params[2] = {};
    params[kRootParamMvp].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    params[kRootParamMvp].Constants.Num32BitValues = kMvpConstantCount;
    params[kRootParamMvp].Constants.ShaderRegister = 0; // b0
    params[kRootParamMvp].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    params[kRootParamTexture].ParameterType =
        D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[kRootParamTexture].DescriptorTable.NumDescriptorRanges = 1;
    params[kRootParamTexture].DescriptorTable.pDescriptorRanges = &srvRange;
    params[kRootParamTexture].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT; // 点采样,回读确定
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.ShaderRegister = 0; // s0
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
    rsDesc.NumParameters = 2;
    rsDesc.pParameters = params;
    rsDesc.NumStaticSamplers = 1;
    rsDesc.pStaticSamplers = &sampler;
    rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> rsBlob, rsErr;
    if (FAILED(D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1,
                                           &rsBlob, &rsErr))) {
        return nullptr;
    }
    if (FAILED(dev->CreateRootSignature(0, rsBlob->GetBufferPointer(),
                                        rsBlob->GetBufferSize(),
                                        IID_PPV_ARGS(&self->m_rootSig)))) {
        return nullptr;
    }

    // 3) 输入布局 + PSO。
    D3D12_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso = {};
    pso.pRootSignature = self->m_rootSig.Get();
    pso.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
    pso.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
    pso.InputLayout = {layout, 2};
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.NumRenderTargets = 1;
    pso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pso.SampleDesc.Count = 1;
    pso.SampleMask = UINT_MAX;
    // 光栅化:M1 关背面剔除,绕序无关(学习优先)。
    pso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pso.RasterizerState.DepthClipEnable = TRUE;
    // 混合:不透明直写;深度/模板关闭。
    for (auto& rt : pso.BlendState.RenderTarget) {
        rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    }
    pso.DepthStencilState.DepthEnable = FALSE;
    pso.DepthStencilState.StencilEnable = FALSE;

    if (FAILED(dev->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&self->m_pso)))) {
        return nullptr;
    }

    // 4) 单位四边形 VB/IB(上传堆)。
    const auto verts = UnitQuadVertices();
    const auto indices = UnitQuadIndices();
    self->m_vb = GpuBuffer::CreateUpload(dev, verts.data(),
                                         verts.size() * sizeof(SpriteVertex));
    self->m_ib = GpuBuffer::CreateUpload(dev, indices.data(),
                                         indices.size() * sizeof(uint16_t));
    if (!self->m_vb || !self->m_ib) return nullptr;

    self->m_vbv.BufferLocation = self->m_vb->Gpu();
    self->m_vbv.SizeInBytes = static_cast<UINT>(self->m_vb->Size());
    self->m_vbv.StrideInBytes = sizeof(SpriteVertex);
    self->m_ibv.BufferLocation = self->m_ib->Gpu();
    self->m_ibv.SizeInBytes = static_cast<UINT>(self->m_ib->Size());
    self->m_ibv.Format = DXGI_FORMAT_R16_UINT;
    return self;
}

SpriteRenderer::~SpriteRenderer() = default;

void SpriteRenderer::Draw(ID3D12GraphicsCommandList* cmd,
                          const me::rhi::GpuTexture& tex,
                          const me::Matrix4x4& mvp) {
    cmd->SetPipelineState(m_pso.Get());
    cmd->SetGraphicsRootSignature(m_rootSig.Get());
    // 行主序 m[4][4] 的内存布局即 row0..row3,与 HLSL row_major 一致,直接灌入。
    cmd->SetGraphicsRoot32BitConstants(0, 16, &mvp, 0);
    cmd->SetGraphicsRootDescriptorTable(1, tex.SrvGpu());
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmd->IASetVertexBuffers(0, 1, &m_vbv);
    cmd->IASetIndexBuffer(&m_ibv);
    cmd->DrawIndexedInstanced(me::rhi::kSpriteIndexCount, 1, 0, 0, 0);
}

} // namespace me::render
```

- [ ] **Step 4: 写 `engine/renderer/CMakeLists.txt` 与 `README.md`**

```cmake
if(WIN32)
    add_library(me_renderer STATIC
        src/SpriteRenderer.cpp
    )
    target_include_directories(me_renderer PUBLIC include)
    target_compile_features(me_renderer PUBLIC cxx_std_17)
    # 单向依赖:renderer → core, rhi(含 me_rhi_cpu), assets。
    target_link_libraries(me_renderer PUBLIC me_core me_rhi me_rhi_cpu me_assets)
endif()
```

`engine/renderer/README.md`:
```markdown
# engine/renderer(me_renderer,仅 Windows)

2D 渲染。M1 仅含 SpriteRenderer(单精灵:根签名+PSO+单位四边形,经 MVP 绘制)。
依赖:Core, RHI, Assets(单向)。M2 扩展为 SpriteBatch + 正交相机。
```

- [ ] **Step 5: 接入构建与 GPU 测试**

根 `CMakeLists.txt` 在 `add_subdirectory(engine/assets)` 后加 `add_subdirectory(engine/renderer)`(置于 `engine/rhi` 之后,满足依赖顺序)。
`engine/rhi/CMakeLists.txt` 中 `me_gpu_tests`:源追加 `${CMAKE_SOURCE_DIR}/tests/gpu/test_sprite_render.cpp`,`target_link_libraries` 追加 `me_renderer`。

> 顺序提示:`me_renderer` 在 `engine/renderer/` 定义,而 `me_gpu_tests` 在 `engine/rhi/` 定义且要链接它。CMake target 链接不要求定义顺序(生成期解析),但 `add_subdirectory(engine/renderer)` 必须在根 `CMakeLists.txt` 中先于或独立于 `engine/rhi` 的测试目标创建即可——两者都在配置阶段注册,链接在生成期解析,无先后问题。

- [ ] **Step 6: 运行 GPU 测试确认通过 [Windows]**

Run:
```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DME_BUILD_TESTS=ON -DME_BUILD_GPU_TESTS=ON
cmake --build build --config Debug
ctest --test-dir build -C Debug -R me_gpu_tests --output-on-failure
```
Expected: 含"带纹理精灵"用例的 `me_gpu_tests` 全 PASS(中心红、四角蓝)。

- [ ] **Step 7: 提交**

```bash
git add engine/renderer tests/gpu/test_sprite_render.cpp engine/rhi/CMakeLists.txt CMakeLists.txt
git commit -m "feat(renderer): SpriteRenderer(根签名+PSO+四边形)+ 精灵渲染回读测试"
```

---

### Task 13: Sandbox 上屏(窗口 + 输入 + 交换链 + 精灵)+ Win32 键映射

把全部部件接起来:开窗、加载 PNG、每帧清屏并画精灵、WASD 平移、ESC 退出。这是 M1 的**真机目视终验**(也顺带验证 Task 1 的窗口与 Task 2 的输入)。

**Files:**
- Create: `engine/platform/src/Input_Win32.cpp`(VK→KeyCode 映射)
- Modify: `engine/platform/include/me/platform/Input.h`(加内部映射声明)、`engine/platform/include/me/platform/Window.h` + `engine/platform/src/Window_Win32.cpp`(键消息馈入 InputState)、`engine/platform/CMakeLists.txt`
- Create: `assets/textures/test_sprite.png`
- Create: `sandbox/main.cpp`, `sandbox/CMakeLists.txt`
- Modify: `CMakeLists.txt`(`add_subdirectory(sandbox)`,仅 Windows)

**Interfaces:**
- Consumes: 之前所有 RHI/renderer/assets/platform 类型。
- Produces:
  - `std::optional<me::platform::KeyCode> me::platform::detail::MapPlatformKey(unsigned int nativeKey);`
  - `void me::platform::Window::SetInput(InputState* input);` // 注册后窗口把键消息喂入

- [ ] **Step 1: 在 `Input.h` 末尾(命名空间内)加入映射声明**

```cpp
#include <optional> // 置于文件顶部 include 区

namespace me::platform::detail {
/** @brief 平台原生按键码(Win32 VK)→ KeyCode;不关心的键返回 nullopt。 */
std::optional<KeyCode> MapPlatformKey(unsigned int nativeKey);
} // namespace me::platform::detail
```

- [ ] **Step 2: 写 `Input_Win32.cpp`**

```cpp
#include "me/platform/Input.h"

#include <windows.h>

namespace me::platform::detail {

std::optional<KeyCode> MapPlatformKey(unsigned int vk) {
    switch (vk) {
    case VK_ESCAPE: return KeyCode::Escape;
    case VK_SPACE:  return KeyCode::Space;
    case 'W':       return KeyCode::W;
    case 'A':       return KeyCode::A;
    case 'S':       return KeyCode::S;
    case 'D':       return KeyCode::D;
    default:        return std::nullopt;
    }
}

} // namespace me::platform::detail
```

- [ ] **Step 3: 扩展 `Window`:注册 InputState 并在 WndProc 馈入键**

`Window.h` 公有区加入(并在文件顶部前置声明 `class InputState;`):
```cpp
    /** @brief 注册键盘状态机;注册后窗口把 WM_KEYDOWN/UP 翻译并喂入。 */
    void SetInput(InputState* input);
```
`Window_Win32.cpp`:`#include "me/platform/Input.h"`;`struct Impl` 增加 `InputState* input = nullptr;`;在 `WndProc` 的 `if (impl != nullptr)` 分支内追加:
```cpp
        case WM_KEYDOWN:
            if (impl->input && (lparam & (1 << 30)) == 0) { // 过滤自动重复
                if (auto k = detail::MapPlatformKey(static_cast<unsigned>(wparam)))
                    impl->input->OnKeyDown(*k);
            }
            return 0;
        case WM_KEYUP:
            if (impl->input) {
                if (auto k = detail::MapPlatformKey(static_cast<unsigned>(wparam)))
                    impl->input->OnKeyUp(*k);
            }
            return 0;
```
并在文件内实现:
```cpp
void Window::SetInput(InputState* input) { m_impl->input = input; }
```

- [ ] **Step 4: 让 Win32 输入源参与编译**

`engine/platform/CMakeLists.txt` 的 `if(WIN32)` 块的 `target_sources` 追加 `src/Input_Win32.cpp`。

- [ ] **Step 5: 放入测试贴图 `assets/textures/test_sprite.png`**

放任意 PNG 即可。若需即时生成一张 64x64 可辨识图(对角渐变 + 中心方块):
```bash
mkdir -p assets/textures
python3 - <<'PY'
import struct, zlib
w=h=64
def px(x,y):
    if 24<=x<40 and 24<=y<40: return (255,255,255)   # 中心白块
    return (x*4%256, y*4%256, 128)                    # 渐变背景
raw=b''.join(b'\x00'+b''.join(struct.pack("BBB",*px(x,y)) for x in range(w)) for y in range(h))
def chunk(t,d):
    c=t+d; return struct.pack(">I",len(d))+c+struct.pack(">I",zlib.crc32(c)&0xffffffff)
png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack(">IIBBBBB",w,h,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(raw))+chunk(b'IEND',b'')
open("assets/textures/test_sprite.png","wb").write(png)
print("wrote assets/textures/test_sprite.png")
PY
```

- [ ] **Step 6: 写 `sandbox/main.cpp`**

```cpp
#include <memory>

#include "me/platform/Window.h"
#include "me/platform/Input.h"
#include "me/assets/ImageData.h"
#include "me/core/Log.h"
#include "me/core/Matrix4x4.h"
#include "me/core/Vector2.h"

#include "me/rhi/GpuDevice.h"
#include "me/rhi/SwapChain.h"
#include "me/rhi/Fence.h"
#include "me/rhi/CommandContext.h"
#include "me/rhi/DescriptorHeap.h"
#include "me/rhi/GpuTexture.h"
#include "me/rhi/SpriteTransform.h"
#include "me/render/SpriteRenderer.h"

#include <d3d12.h>

using namespace me;

namespace {
constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr float kSpritePixels = 256.0f; // 精灵边长(像素)
constexpr float kMoveSpeedPixels = 8.0f; // 每帧平移步长

void Transition(ID3D12GraphicsCommandList* cmd, ID3D12Resource* res,
                D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to) {
    D3D12_RESOURCE_BARRIER b = {};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource = res;
    b.Transition.StateBefore = from;
    b.Transition.StateAfter = to;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmd->ResourceBarrier(1, &b);
}
} // namespace

int main() {
    platform::WindowDesc wd;
    wd.width = kWindowWidth;
    wd.height = kWindowHeight;
    wd.title = "MiniEngine M1 — Sprite";
    auto window = platform::Window::Create(wd);
    if (!window) { ME_LOG_ERROR("窗口创建失败"); return 1; }

    platform::InputState input;
    window->SetInput(&input);

    auto device = rhi::GpuDevice::Create(/*useWarp=*/false);
    if (!device) device = rhi::GpuDevice::Create(/*useWarp=*/true); // 回退软件
    if (!device) { ME_LOG_ERROR("DX12 设备创建失败"); return 1; }

    auto swapChain = rhi::SwapChain::Create(*device, window->NativeHandle(),
                                            kWindowWidth, kWindowHeight);
    auto ctx = rhi::CommandContext::Create(device->Device());
    auto fence = rhi::Fence::Create(device->Device());
    auto srvHeap = rhi::DescriptorHeap::Create(
        device->Device(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 8, true);
    if (!swapChain || !ctx || !fence || !srvHeap) {
        ME_LOG_ERROR("RHI 初始化失败"); return 1;
    }

    auto image = assets::LoadImageRGBA8(std::string(ME_ASSET_DIR) +
                                        "/textures/test_sprite.png");
    if (!image) { ME_LOG_ERROR("加载贴图失败"); return 1; }

    auto srv = srvHeap->Allocate();
    auto texture = rhi::GpuTexture::Create(
        device->Device(), device->Queue(), *fence,
        static_cast<uint32_t>(image->width), static_cast<uint32_t>(image->height),
        image->pixels.data(), srv);
    auto renderer = render::SpriteRenderer::Create(*device);
    if (!texture || !renderer) { ME_LOG_ERROR("纹理/渲染器创建失败"); return 1; }

    // 世界空间正交投影:左下原点,Y 向上,单位=像素。
    const Matrix4x4 proj = Matrix4x4::Orthographic(
        0.0f, float(kWindowWidth), 0.0f, float(kWindowHeight), 0.0f, 1.0f);
    Vector2 spritePos{float(kWindowWidth) * 0.5f, float(kWindowHeight) * 0.5f};

    while (!window->ShouldClose()) {
        input.NewFrame();
        window->PumpMessages();
        if (input.WasPressed(platform::KeyCode::Escape)) break;
        if (input.IsDown(platform::KeyCode::A)) spritePos.x -= kMoveSpeedPixels;
        if (input.IsDown(platform::KeyCode::D)) spritePos.x += kMoveSpeedPixels;
        if (input.IsDown(platform::KeyCode::W)) spritePos.y += kMoveSpeedPixels;
        if (input.IsDown(platform::KeyCode::S)) spritePos.y -= kMoveSpeedPixels;

        auto* cmd = ctx->Begin();
        ID3D12Resource* back = swapChain->CurrentBackBuffer();
        D3D12_CPU_DESCRIPTOR_HANDLE rtv = swapChain->CurrentRtv();
        Transition(cmd, back, D3D12_RESOURCE_STATE_PRESENT,
                   D3D12_RESOURCE_STATE_RENDER_TARGET);

        D3D12_VIEWPORT vp = {0, 0, float(kWindowWidth), float(kWindowHeight), 0.0f, 1.0f};
        D3D12_RECT scissor = {0, 0, kWindowWidth, kWindowHeight};
        cmd->RSSetViewports(1, &vp);
        cmd->RSSetScissorRects(1, &scissor);
        cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
        const float clearColor[4] = {0.10f, 0.12f, 0.16f, 1.0f}; // 暗灰蓝清屏
        cmd->ClearRenderTargetView(rtv, clearColor, 0, nullptr);

        ID3D12DescriptorHeap* heaps[] = {srvHeap->Heap()};
        cmd->SetDescriptorHeaps(1, heaps);

        const Matrix4x4 model = rhi::MakeSpriteModelMatrix(
            spritePos, Vector2{kSpritePixels, kSpritePixels}, 0.0f);
        const Matrix4x4 mvp = model * proj; // 行向量:v * model * proj
        renderer->Draw(cmd, *texture, mvp);

        Transition(cmd, back, D3D12_RESOURCE_STATE_RENDER_TARGET,
                   D3D12_RESOURCE_STATE_PRESENT);
        ctx->End();
        ctx->Execute(device->Queue());
        swapChain->Present();
        fence->Flush(device->Queue()); // M1 简单同步:每帧等 GPU(M2 再做并行化)
        ctx->AdvanceFrame();
    }

    fence->Flush(device->Queue()); // 退出前确保 GPU 空闲再析构资源
    return 0;
}
```

- [ ] **Step 7: 写 `sandbox/CMakeLists.txt`**

```cmake
add_executable(sandbox main.cpp)
target_compile_features(sandbox PRIVATE cxx_std_17)
target_link_libraries(sandbox PRIVATE
    me_core me_platform me_rhi me_rhi_cpu me_renderer me_assets)
# ME_ASSET_DIR 经 me_rhi 的 PUBLIC 编译定义传递,sandbox 直接可用。
```

- [ ] **Step 8: 接入根 CMake(仅 Windows 构建 sandbox)**

根 `CMakeLists.txt` 末尾(`if(ME_BUILD_TESTS)` 之后)加:
```cmake
if(WIN32)
    add_subdirectory(sandbox)
endif()
```

- [ ] **Step 9: WSL 全量逻辑测试仍绿 [WSL]**

Run: `cmake -S . -B build-wsl -DME_BUILD_TESTS=ON -DME_BUILD_GPU_TESTS=OFF && cmake --build build-wsl -j && ctest --test-dir build-wsl --output-on-failure`
Expected: 全部跨平台单测 PASS(sandbox/me_rhi 等 Windows-only 目标不参与)。

- [ ] **Step 10: Windows 构建并目视验证 [Windows]**

Run:
```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DME_BUILD_TESTS=ON -DME_BUILD_GPU_TESTS=ON
cmake --build build --config Debug
.\build\bin\Debug\sandbox.exe
```
Expected(逐项确认并记录):
1. 弹出 1280×720 标题为 "MiniEngine M1 — Sprite" 的窗口。
2. 暗灰蓝背景中央显示测试贴图精灵(256px 见方,贴图正立、不上下翻转)。
3. 按住 W/A/S/D 精灵分别上/左/下/右平移(方向与世界 Y 向上一致)。
4. 按 ESC 或点关闭按钮窗口退出,进程干净结束(无 DX12 调试层报错)。

- [ ] **Step 11: 提交**

```bash
git add engine/platform sandbox assets/textures/test_sprite.png CMakeLists.txt
git commit -m "feat(sandbox): M1 精灵上屏(窗口+输入+交换链+SpriteRenderer)+ Win32 键映射"
```

---

### Task 14: 进度回写与文档收尾

把 M1 的完成状态写回进度索引与 README,记录关键决策(对应 brainstorm 里的三个 M1 决策)。

**Files:**
- Modify: `docs/PROGRESS.md`、`README.md`

- [ ] **Step 1: 更新 `docs/PROGRESS.md`**

- 顶部"最后更新"改为 `2026-06-18`,"当前阶段"改为 `M1 精灵上屏完成,下一步 M2 批渲染 + 正交相机`。
- 里程碑表 `M1 精灵上屏` 状态 `☐` → `☑`,说明补"Win32 Window/Input + RHI(Device/SwapChain/CmdList/Fence/PSO)+ stb_image 纹理 + SpriteRenderer;WARP 像素回读 + sandbox 目视验证"。
- "一句话现状"改写为 M1 完成态。
- ADR 表追加三行(日期 2026-06-18):
  - `M1 着色器用 FXC(D3DCompileFromFile,SM5.1)而非 DXC` — 理由:零额外依赖、最快上屏,DXC 留到需 SM6 时。
  - `M1 用系统 DX12(d3d12/dxgi/d3dcompiler),不引入 Agility SDK` — 理由:最小依赖,够用即可。
  - `GPU 代码用 WARP 软件适配器 + 离屏像素回读做自动化测试,辅以 sandbox 目视` — 理由:在无独显/无窗口环境也能红绿,补足 RHI 不可纯 CPU 单测的空缺。
- "待解决/开放问题"移除"起始里程碑未定";可加"上传堆顶点/纹理为 M1 简化,M2/性能里程碑改默认堆"。

- [ ] **Step 2: 更新 `README.md`**

- 模块清单补 `engine/rhi`(`me_rhi`/`me_rhi_cpu`)、`engine/assets`、`engine/renderer`、`sandbox`。
- 新增"Windows 构建(M1)"小节,贴本计划"构建与验证环境"里的两套命令(WSL 逻辑 / Windows GPU+目视)。

- [ ] **Step 3: 提交**

```bash
git add docs/PROGRESS.md README.md
git commit -m "docs(m1): 进度回写 + ADR(FXC/系统DX12/WARP测试)+ README 构建说明"
```

---

## 自检(Self-Review)

**Spec 覆盖**(对照设计文档 §13 的 M1 行 "RHI 最小可用(Device/SwapChain/CmdList/Fence/PSO)→ 清屏 → 画一个带纹理精灵",及 ADR "Win32 Window/Input 推迟到 M1"):
- Device → Task 5;Fence → Task 5;CmdList/CommandAllocator → Task 6;SwapChain → Task 11;PSO/RootSignature → Task 12;清屏 → Task 12 测试 + Task 13 sandbox;带纹理精灵 → Task 8(纹理)+ Task 12(绘制)+ Task 13(目视);Win32 Window → Task 1;Input → Task 2 + Task 13。**全部覆盖。**
- 数据驱动:贴图/着色器路径来自 `ME_ASSET_DIR` 常量,无硬编码内容字符串 ✔;魔法数字均具名(`kFrameCount`/`kSpritePixels`/`kMvpConstantCount`…)✔。
- 测试策略:CPU 逻辑(Task 3/10,WSL)+ WARP 回读(Task 5/8/12,Windows)+ sandbox 目视(Task 13)✔。

**类型一致性**:`GpuDevice::Device()/Queue()/Factory()`、`Fence::Signal/Wait/Flush/CompletedValue`、`Descriptor{cpu,gpu,index}`、`DescriptorHeap::Allocate/Heap`、`CommandContext::Begin/End/Execute/AdvanceFrame`、`GpuBuffer::Gpu/Size/Resource`、`GpuTexture::Create/Resource/Width/Height/SrvGpu`、`ReadbackRgba8(...)`、`CompileHlsl(...)`、`SpriteRenderer::Create/Draw`、`UnitQuadVertices/Indices`、`MakeSpriteModelMatrix` —— 各任务引用与定义签名一致 ✔。`kSpriteIndexCount` 在 Task 3 定义、Task 12 `Draw` 使用 ✔。

**占位符扫描**:无 TBD/TODO;每个改码步骤含完整代码与可运行命令;测试步骤含期望输出 ✔。唯一需执行者动手生成内容的是两处真实 PNG 字节(Task 10 内嵌测试图、Task 13 磁盘贴图),均给出确定性 Python 生成脚本,非占位 ✔。

**已知风险(执行时留意)**:
- DX12 代码在 WSL 无法编译,Windows 侧首次编译可能暴露 SDK 头细节差异(如 `dxgi1_6.h` 可用性);若链接缺符号,确认 `target_link_libraries` 含 `d3d12 dxgi d3dcompiler dxguid`。
- WARP 在部分 CI/无图形栈机器上仍需 Windows 图形组件;`me_gpu_tests` 仅在 `ME_BUILD_GPU_TESTS=ON` 且 Windows 下构建。
- `stb` 用 `master` tag,FetchContent 首次拉取需网络;离线时预先 populate。

## Execution Handoff

Plan complete and saved to `docs/superpowers/plans/2026-06-18-m1-sprite-on-screen.md`. Two execution options:

**1. Subagent-Driven (recommended)** — 每个任务派一个全新 subagent 实现,任务间我来审查,迭代快。

**2. Inline Execution** — 在本会话内用 executing-plans 批量执行,带检查点复查。

> ⚠️ 注意:Task 5/8/11/12/13 含 DX12/Win32,**必须在 Windows(VS 2022 + Windows SDK)上编译与验证**;WSL 只能跑 Task 1–3、10 的跨平台逻辑测试。请确认执行环境。

Which approach?
