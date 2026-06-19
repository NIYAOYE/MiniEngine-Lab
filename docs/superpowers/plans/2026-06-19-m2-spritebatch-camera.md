# M2 批渲染 + 正交相机 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 M1 单精灵的基础上,实现按纹理合批的 `SpriteBatch` + 可平移/缩放的 `OrthographicCamera`,使多精灵经一相机一次合批上屏。

**Architecture:** 新增 header-only `OrthographicCamera`(由 position/zoom/viewport 构造正交 `viewProj`,纯 CPU 可测)、POD 提交单元 `SpriteDesc`、以及拥有 PSO/根签名/动态顶点缓冲的 `SpriteBatch`(`Begin/Submit/End`,按纹理指针稳定排序后逐 run 合批)。模型变换在 CPU 端烘进顶点,顶点格式 `pos+uv+color`。M1 的 `SpriteRenderer` 退役并入 `SpriteBatch`。延续 M1 的上传堆与每帧 `fence->Flush` 全同步;帧并行、默认堆、AssetManager 均不在本里程碑。

**Tech Stack:** C++17、DirectX 12(系统 d3d12/dxgi/d3dcompiler)、FXC(SM5.1)、doctest、WARP 软件适配器 + 离屏像素回读。

## Global Constraints

逐条来自 CLAUDE.md 与 M2 spec,**每个任务都隐含适用**:
- **零魔法数字**:所有数值常量必须具名 `constexpr` 或来自配置;禁止裸数字。
- **有注释**:每个公开 API 写 Doxygen 注释;非显而易见实现写行内注释。
- **不使用 C++ 异常**:可恢复错误用返回值 / `std::optional` / 空 `unique_ptr`;不变量违反用 `ME_ASSERT` / `ME_ASSERT_MSG`;DX12 `HRESULT` 用 `ME_HR_CHECK`(或现有 `FAILED(...) return nullptr` 模式)。
- **命名**:`PascalCase` 类型/函数,`m_camelCase` 成员,`s_`/`g_` 静态/全局;头文件 `#pragma once`;命名空间 `me::render` / `me::rhi`。头文件中禁止 `using namespace std`。
- **单向依赖**:`renderer → core, rhi, assets`;裸 `ID3D12*` 不出 RHI 层(`SpriteBatch` 录制命令时接收 `ID3D12GraphicsCommandList*` 与 RHI 封装对象,符合 M1 既有边界)。
- **数学约定**:行主序存储 + 行向量(`v' = v * M`);世界空间 Y 轴向上、像素为单位、原点左下;纹理 V 向下(底边对应 vMax)。
- **新增模块/源文件必须同步更新对应 `CMakeLists.txt` 与模块 `README.md`。**
- **构建/测试命令(WSL interop)**:
  - 跨平台逻辑(含相机 doctest):
    ```bash
    cmake -S . -B build-wsl -DME_BUILD_TESTS=ON -DME_BUILD_GPU_TESTS=OFF
    cmake --build build-wsl -j
    ctest --test-dir build-wsl --output-on-failure
    ```
  - Windows DX12/GPU(WARP 像素回读 + sandbox 目视),在 Developer PowerShell:
    ```powershell
    cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DME_BUILD_TESTS=ON -DME_BUILD_GPU_TESTS=ON
    cmake --build build --config Debug
    ctest --test-dir build -C Debug --output-on-failure
    .\build\bin\Debug\sandbox.exe
    ```

---

## 文件结构总览

| 文件 | 职责 | 任务 |
|------|------|------|
| `engine/rhi/include/me/rhi/GpuBuffer.h` / `src/GpuBuffer.cpp`(改) | 增加持久映射的动态上传缓冲(`CreateDynamic`/`Write`/`Mapped`/析构解映射) | T1 |
| `tests/gpu/test_dynamic_buffer.cpp`(新) | 动态缓冲 Write→Mapped 回读 GPU 测试 | T1 |
| `engine/renderer/include/me/render/OrthographicCamera.h`(新,header-only) | position/zoom/viewport → `viewProj`,纯 CPU | T2 |
| `tests/render/test_camera.cpp`(新) | 相机映射 doctest(WSL 可跑) | T2 |
| `engine/renderer/include/me/render/SpriteDesc.h`(新) | POD 提交单元 | T3 |
| `engine/renderer/include/me/render/SpriteBatch.h` / `src/SpriteBatch.cpp`(新) | 合批渲染器,拥有 PSO/根签名/动态 VB/静态 IB | T3、T4 |
| `assets/shaders/sprite.hlsl`(改) | 顶点加 `color`,`uMVP`→`uViewProj`,PS 乘色调 | T3 |
| `engine/renderer/include/me/render/SpriteRenderer.h` / `src/SpriteRenderer.cpp`(删) | M1 单精灵渲染器退役 | T3 |
| `engine/renderer/CMakeLists.txt`(改) | 源文件 `SpriteRenderer.cpp`→`SpriteBatch.cpp` | T3 |
| `tests/gpu/test_sprite_render.cpp`(改) | 改用 `SpriteBatch` 渲染单精灵 + 色调 | T3 |
| `tests/gpu/test_sprite_batch.cpp`(新) | 多精灵合批:DrawCallCount 分组、srcRect、多纹理、容量增长 | T4 |
| `engine/rhi/CMakeLists.txt`(改) | `me_gpu_tests` 增加 `test_dynamic_buffer.cpp`、`test_sprite_batch.cpp` | T1、T4 |
| `tests/CMakeLists.txt`(改) | `me_tests` 增加 `render/test_camera.cpp` + renderer include 路径 | T2 |
| `sandbox/main.cpp`(改) | 多静态精灵 + 相机 WASD 平移 / Q/E 缩放 / ESC | T5 |
| `engine/renderer/README.md` / `docs/PROGRESS.md`(改) | 模块说明 + 进度回写 | T6 |

---

## Task 1: RHI 动态上传缓冲(持久映射)

**Files:**
- Modify: `engine/rhi/include/me/rhi/GpuBuffer.h`
- Modify: `engine/rhi/src/GpuBuffer.cpp`
- Test: `tests/gpu/test_dynamic_buffer.cpp`(新)
- Modify: `engine/rhi/CMakeLists.txt:31-36`(`me_gpu_tests` 源列表)

**Interfaces:**
- Consumes: 现有 `GpuBuffer::CreateUpload(ID3D12Device*, const void*, size_t)`、`Gpu()`、`Size()`、`Resource()`。
- Produces:
  - `static std::unique_ptr<GpuBuffer> GpuBuffer::CreateDynamic(ID3D12Device* device, size_t capacityBytes);` —— 上传堆、构造后**保持映射**。
  - `void GpuBuffer::Write(const void* data, size_t bytes, size_t offsetBytes = 0);`
  - `const uint8_t* GpuBuffer::Mapped() const;` —— 持久映射指针(`CreateUpload` 创建的对象返回 `nullptr`)。
  - 析构时若仍映射则 `Unmap`。

- [ ] **Step 1: 在 `me_gpu_tests` 注册新测试文件并写失败测试**

先把测试文件加入构建。编辑 `engine/rhi/CMakeLists.txt`,在 `add_executable(me_gpu_tests ...)` 的源列表中(第 35 行 `test_sprite_render.cpp` 之后)追加一行:

```cmake
            ${CMAKE_SOURCE_DIR}/tests/gpu/test_dynamic_buffer.cpp
```

新建 `tests/gpu/test_dynamic_buffer.cpp`:

```cpp
#include <doctest/doctest.h>
#include <cstdint>

#include "me/rhi/GpuDevice.h"
#include "me/rhi/GpuBuffer.h"

using namespace me::rhi;

namespace {
constexpr size_t kCapBytes = 256; // 动态缓冲容量(字节)
} // namespace

TEST_CASE("动态上传缓冲:Write 后经 Mapped 可读回相同字节") {
    auto device = GpuDevice::Create(/*useWarp=*/true);
    REQUIRE(device != nullptr);

    auto buf = GpuBuffer::CreateDynamic(device->Device(), kCapBytes);
    REQUIRE(buf != nullptr);
    REQUIRE(buf->Mapped() != nullptr);

    const uint32_t pattern[4] = {0xAABBCCDDu, 1u, 2u, 3u};
    buf->Write(pattern, sizeof(pattern), 0);

    // 上传堆 CPU 可见:持久映射指针应反映刚写入的数据。
    const auto* got = reinterpret_cast<const uint32_t*>(buf->Mapped());
    CHECK(got[0] == 0xAABBCCDDu);
    CHECK(got[3] == 3u);
}
```

- [ ] **Step 2: 配置 + 构建,确认编译失败**

Run(Windows / Developer PowerShell):
```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DME_BUILD_TESTS=ON -DME_BUILD_GPU_TESTS=ON
cmake --build build --config Debug --target me_gpu_tests
```
Expected: 编译失败 —— `CreateDynamic` / `Write` / `Mapped` 未声明。

- [ ] **Step 3: 在头文件声明新接口**

编辑 `engine/rhi/include/me/rhi/GpuBuffer.h`,把 `class GpuBuffer` 改为:

```cpp
class GpuBuffer {
public:
    /// @brief 上传堆缓冲:创建即写入 data 并解映射(M1 顶点/索引用)。
    static std::unique_ptr<GpuBuffer> CreateUpload(ID3D12Device* device,
                                                   const void* data,
                                                   size_t sizeBytes);

    /// @brief 动态上传堆缓冲:容量 capacityBytes,构造后保持持久映射,供每帧 Write。
    static std::unique_ptr<GpuBuffer> CreateDynamic(ID3D12Device* device,
                                                    size_t capacityBytes);

    ~GpuBuffer();

    /// @brief 把 bytes 字节从 data 拷入映射区 offsetBytes 处(仅动态缓冲有效)。
    void Write(const void* data, size_t bytes, size_t offsetBytes = 0);

    D3D12_GPU_VIRTUAL_ADDRESS Gpu() const { return m_resource->GetGPUVirtualAddress(); }
    size_t Size() const { return m_size; }
    ID3D12Resource* Resource() const { return m_resource.Get(); }
    /// @brief 持久映射指针;CreateUpload 创建的对象返回 nullptr。
    const uint8_t* Mapped() const { return m_mapped; }

private:
    GpuBuffer() = default;
    ComPtr<ID3D12Resource> m_resource;
    size_t m_size = 0;
    uint8_t* m_mapped = nullptr; // 动态缓冲持久映射;CreateUpload 为 nullptr
};
```

- [ ] **Step 4: 在 .cpp 实现 `CreateDynamic` / `Write` / 析构**

编辑 `engine/rhi/src/GpuBuffer.cpp`,在 `#include <cstring>` 之后加入 `#include "me/core/Assert.h"`,并在 `CreateUpload` 之后追加:

```cpp
std::unique_ptr<GpuBuffer> GpuBuffer::CreateDynamic(ID3D12Device* device,
                                                    size_t capacityBytes) {
    auto self = std::unique_ptr<GpuBuffer>(new GpuBuffer());
    self->m_size = capacityBytes;

    D3D12_HEAP_PROPERTIES heap = {};
    heap.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = capacityBytes;
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

    // 持久映射:整个生命周期保持映射,避免每帧 Map/Unmap 开销。
    D3D12_RANGE noRead = {0, 0}; // CPU 不读
    void* mapped = nullptr;
    if (FAILED(self->m_resource->Map(0, &noRead, &mapped))) {
        return nullptr;
    }
    self->m_mapped = static_cast<uint8_t*>(mapped);
    return self;
}

void GpuBuffer::Write(const void* data, size_t bytes, size_t offsetBytes) {
    ME_ASSERT_MSG(m_mapped != nullptr, "GpuBuffer::Write: 仅动态缓冲可写");
    ME_ASSERT_MSG(offsetBytes + bytes <= m_size, "GpuBuffer::Write: 越界");
    std::memcpy(m_mapped + offsetBytes, data, bytes);
}

GpuBuffer::~GpuBuffer() {
    if (m_mapped != nullptr && m_resource) {
        m_resource->Unmap(0, nullptr);
        m_mapped = nullptr;
    }
}
```

- [ ] **Step 5: 重新构建并运行,确认通过**

Run:
```powershell
cmake --build build --config Debug --target me_gpu_tests
ctest --test-dir build -C Debug -R me_gpu_tests --output-on-failure
```
Expected: PASS —— 含「动态上传缓冲:Write 后经 Mapped 可读回相同字节」。

- [ ] **Step 6: Commit**

```bash
git add engine/rhi/include/me/rhi/GpuBuffer.h engine/rhi/src/GpuBuffer.cpp \
        tests/gpu/test_dynamic_buffer.cpp engine/rhi/CMakeLists.txt
git commit -m "feat(rhi): GpuBuffer 动态持久映射上传缓冲(CreateDynamic/Write/Mapped)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 2: OrthographicCamera(header-only,纯 CPU 可测)

**Files:**
- Create: `engine/renderer/include/me/render/OrthographicCamera.h`
- Test: `tests/render/test_camera.cpp`(新)
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `me::Matrix4x4::Orthographic(left,right,bottom,top,nearZ,farZ)`、`me::Vector2`。
- Produces:
  - `me::render::OrthographicCamera`,字段经构造/设值:`SetViewportSize(float w, float h)`、`SetPosition(me::Vector2)`、`SetZoom(float)`、`me::Matrix4x4 ViewProj() const`。
  - 语义:`viewProj` 把世界点 `position` 映射到 NDC 原点;可见半宽 = `(viewportWidth * 0.5) / zoom`,半高同理。`zoom>1` 放大(可见范围变小)。

- [ ] **Step 1: 注册测试并加 renderer include 路径,写失败测试**

编辑 `tests/CMakeLists.txt`:在 `add_executable(me_tests ...)` 源列表末尾(`assets/test_image.cpp` 之后)追加:

```cmake
    render/test_camera.cpp
```

并在 `add_test(...)` 之前追加 renderer 头文件路径(相机为 header-only,仅依赖已链接的 `me_core`):

```cmake
target_include_directories(me_tests PRIVATE ${CMAKE_SOURCE_DIR}/engine/renderer/include)
```

新建 `tests/render/test_camera.cpp`:

```cpp
#include <doctest/doctest.h>

#include "me/render/OrthographicCamera.h"
#include "me/core/Matrix4x4.h"
#include "me/core/Vector2.h"

using me::render::OrthographicCamera;
using me::Vector2;

namespace {
constexpr float kEps = 1e-4f;
constexpr float kW = 1280.0f;
constexpr float kH = 720.0f;
} // namespace

TEST_CASE("正交相机:位置映射到 NDC 原点,视口角点映射到 NDC 角点") {
    OrthographicCamera cam;
    cam.SetViewportSize(kW, kH);
    cam.SetPosition(Vector2{640.0f, 360.0f});
    cam.SetZoom(1.0f);
    const me::Matrix4x4 vp = cam.ViewProj();

    const Vector2 center = vp.TransformPoint(Vector2{640.0f, 360.0f});
    CHECK(center.x == doctest::Approx(0.0f).epsilon(kEps));
    CHECK(center.y == doctest::Approx(0.0f).epsilon(kEps));

    // 相机居中、zoom=1:可见范围正好一屏,右上角世界点 (1280,720) → NDC (1,1)。
    const Vector2 topRight = vp.TransformPoint(Vector2{1280.0f, 720.0f});
    CHECK(topRight.x == doctest::Approx(1.0f).epsilon(kEps));
    CHECK(topRight.y == doctest::Approx(1.0f).epsilon(kEps));
}

TEST_CASE("正交相机:zoom 放大使可见半宽减半") {
    OrthographicCamera cam;
    cam.SetViewportSize(kW, kH);
    cam.SetPosition(Vector2{640.0f, 360.0f});
    cam.SetZoom(2.0f);
    const me::Matrix4x4 vp = cam.ViewProj();

    // zoom=2:可见半宽=640/2=320,故世界点 (960,540) 落在 NDC (1,1)。
    const Vector2 edge = vp.TransformPoint(Vector2{960.0f, 540.0f});
    CHECK(edge.x == doctest::Approx(1.0f).epsilon(kEps));
    CHECK(edge.y == doctest::Approx(1.0f).epsilon(kEps));
}

TEST_CASE("正交相机:平移相机后世界原点不再是中心") {
    OrthographicCamera cam;
    cam.SetViewportSize(kW, kH);
    cam.SetPosition(Vector2{0.0f, 0.0f});
    cam.SetZoom(1.0f);
    const me::Matrix4x4 vp = cam.ViewProj();

    // 相机看向世界原点:世界 (0,0) → NDC 原点。
    const Vector2 c = vp.TransformPoint(Vector2{0.0f, 0.0f});
    CHECK(c.x == doctest::Approx(0.0f).epsilon(kEps));
    CHECK(c.y == doctest::Approx(0.0f).epsilon(kEps));
}
```

- [ ] **Step 2: 构建并确认失败**

Run(WSL):
```bash
cmake -S . -B build-wsl -DME_BUILD_TESTS=ON -DME_BUILD_GPU_TESTS=OFF
cmake --build build-wsl -j --target me_tests
```
Expected: 编译失败 —— 找不到 `me/render/OrthographicCamera.h`。

- [ ] **Step 3: 实现 header-only 相机**

新建 `engine/renderer/include/me/render/OrthographicCamera.h`:

```cpp
#pragma once

#include "me/core/Matrix4x4.h"
#include "me/core/Vector2.h"
#include "me/core/Assert.h"

namespace me::render {

/**
 * @brief 2D 正交相机:由中心位置、缩放与视口尺寸生成世界→裁剪的 viewProj。
 *
 * 约定世界空间 Y 轴向上、像素为单位。position 是相机在世界中的中心点;
 * zoom>1 放大(可见范围按比例缩小)。ViewProj() 直接复用 Matrix4x4::Orthographic,
 * 不单独维护 view 矩阵 —— 把可见矩形 [pos±halfExtent] 映射到 NDC。
 */
class OrthographicCamera {
public:
    /// @brief 设置视口像素尺寸(通常等于窗口/渲染目标尺寸)。
    void SetViewportSize(float width, float height) {
        m_width = width;
        m_height = height;
    }
    /// @brief 设置相机中心(世界像素坐标)。
    void SetPosition(const Vector2& position) { m_position = position; }
    /// @brief 设置缩放(>0;1=一屏一比一,>1 放大)。
    void SetZoom(float zoom) { m_zoom = zoom; }

    const Vector2& Position() const { return m_position; }
    float Zoom() const { return m_zoom; }

    /// @brief 生成世界→NDC 的正交矩阵(行向量约定,v' = v * viewProj)。
    Matrix4x4 ViewProj() const {
        ME_ASSERT_MSG(m_zoom > 0.0f, "OrthographicCamera: zoom 必须为正");
        ME_ASSERT_MSG(m_width > 0.0f && m_height > 0.0f,
                      "OrthographicCamera: 视口尺寸必须为正");
        const float halfW = (m_width * 0.5f) / m_zoom;
        const float halfH = (m_height * 0.5f) / m_zoom;
        return Matrix4x4::Orthographic(m_position.x - halfW, m_position.x + halfW,
                                       m_position.y - halfH, m_position.y + halfH,
                                       kNearZ, kFarZ);
    }

private:
    static constexpr float kNearZ = 0.0f;
    static constexpr float kFarZ = 1.0f;
    Vector2 m_position{0.0f, 0.0f};
    float m_zoom = 1.0f;
    float m_width = 1.0f;
    float m_height = 1.0f;
};

} // namespace me::render
```

- [ ] **Step 4: 构建并运行,确认通过**

Run(WSL):
```bash
cmake --build build-wsl -j --target me_tests
ctest --test-dir build-wsl -R me_tests --output-on-failure
```
Expected: PASS —— 三个相机用例全绿。

- [ ] **Step 5: Commit**

```bash
git add engine/renderer/include/me/render/OrthographicCamera.h \
        tests/render/test_camera.cpp tests/CMakeLists.txt
git commit -m "feat(renderer): OrthographicCamera(position/zoom/viewport → viewProj)

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 3: SpriteBatch 核心(单纹理合批 + 色调)+ 着色器 + 退役 SpriteRenderer

**Files:**
- Create: `engine/renderer/include/me/render/SpriteDesc.h`
- Create: `engine/renderer/include/me/render/SpriteBatch.h`
- Create: `engine/renderer/src/SpriteBatch.cpp`
- Modify: `assets/shaders/sprite.hlsl`
- Delete: `engine/renderer/include/me/render/SpriteRenderer.h`、`engine/renderer/src/SpriteRenderer.cpp`
- Modify: `engine/renderer/CMakeLists.txt`
- Modify: `tests/gpu/test_sprite_render.cpp`

**Interfaces:**
- Consumes: `me::rhi::GpuDevice`、`GpuBuffer::CreateDynamic/CreateUpload/Write/Gpu/Size`、`GpuTexture::SrvGpu()`、`CompileHlsl`、`me::Rect`、`me::Vector4`、`me::Matrix4x4`。
- Produces:
  - `me::render::SpriteDesc { const me::rhi::GpuTexture* texture; me::Rect srcRect; me::Rect dstRect; me::Vector4 color; float rotation; }`。
  - `me::render::SpriteBatch`:
    - `static std::unique_ptr<SpriteBatch> Create(me::rhi::GpuDevice& device);`
    - `void Begin(const me::Matrix4x4& viewProj);`
    - `void Submit(const SpriteDesc& sprite);`
    - `void End(ID3D12GraphicsCommandList* cmd);`
    - `size_t DrawCallCount() const;` —— 上次 `End` 发出的 drawcall 数(供 T4 断言)。
  - 常量 `me::render::kInitialSpriteCapacity`(初始可容纳精灵数;超出时缓冲自动增长)。
  - 顶点格式约定:`pos(float2) + uv(float2) + color(float4)`,与 `sprite.hlsl` 输入一致。

- [ ] **Step 1: 改着色器(顶点加 color,uMVP→uViewProj)**

把 `assets/shaders/sprite.hlsl` 整文件替换为:

```hlsl
// 精灵着色器:顶点已在 CPU 端烘入模型变换(世界像素坐标),此处只做世界→裁剪。
// uViewProj 用 row_major 接收,匹配引擎行主序存储 + 行向量约定(v' = v * M)。

cbuffer SpriteConstants : register(b0)
{
    row_major float4x4 uViewProj;
};

Texture2D    gTexture : register(t0);
SamplerState gSampler : register(s0);

struct VSInput
{
    float2 position : POSITION; // 世界像素坐标(已烘模型变换)
    float2 uv       : TEXCOORD0;
    float4 color    : COLOR;    // 每精灵 RGBA 色调
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD0;
    float4 color    : COLOR;
};

PSInput VSMain(VSInput input)
{
    PSInput o;
    o.position = mul(float4(input.position, 0.0f, 1.0f), uViewProj);
    o.uv = input.uv;
    o.color = input.color;
    return o;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return gTexture.Sample(gSampler, input.uv) * input.color;
}
```

- [ ] **Step 2: 写 SpriteDesc.h**

新建 `engine/renderer/include/me/render/SpriteDesc.h`:

```cpp
#pragma once

#include "me/core/Rect.h"
#include "me/core/Vector4.h"

namespace me::rhi { class GpuTexture; }

namespace me::render {

/**
 * @brief 一次精灵绘制的提交单元(POD)。提交给 SpriteBatch::Submit。
 *
 * srcRect:纹理中的采样子区域,归一化 UV(x=uMin,y=vMin(上),width/height 为跨度);
 *          整贴图传 {0,0,1,1}。纹理 V 向下。
 * dstRect:目标世界矩形,像素单位,原点左下、Y 向上(x,y=左下角,width/height=尺寸)。
 * color:RGBA 线性色调,与采样结果相乘;整色传 {1,1,1,1}。
 * rotation:绕 dstRect 中心的弧度旋转。
 * texture:非拥有指针,生命周期归调用方(M2 暂不经 AssetManager)。
 */
struct SpriteDesc {
    const me::rhi::GpuTexture* texture = nullptr;
    me::Rect srcRect{0.0f, 0.0f, 1.0f, 1.0f};
    me::Rect dstRect{};
    me::Vector4 color{1.0f, 1.0f, 1.0f, 1.0f};
    float rotation = 0.0f;
};

} // namespace me::render
```

- [ ] **Step 3: 写 SpriteBatch.h**

新建 `engine/renderer/include/me/render/SpriteBatch.h`:

```cpp
#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include <d3d12.h>

#include "me/rhi/D3DCommon.h"
#include "me/core/Matrix4x4.h"
#include "me/render/SpriteDesc.h"

namespace me::rhi { class GpuDevice; class GpuBuffer; class GpuTexture; }

namespace me::render {

/// 初始可容纳精灵数;单帧提交超出时 VB/IB 自动增长到高水位(不静默丢弃)。
constexpr uint32_t kInitialSpriteCapacity = 1024;

/**
 * @brief 2D 精灵合批渲染器(M2):累积 SpriteDesc → 按纹理稳定排序 → 逐 run 合批绘制。
 *
 * 用法:Begin(viewProj) → 多次 Submit(...) → End(cmd)。模型变换在 CPU 端烘进顶点,
 * 顶点格式 pos+uv+color。调用方须已绑定 RT/视口/裁剪、已清屏、已 SetDescriptorHeaps。
 * 拥有根签名/PSO/动态顶点缓冲/静态四边形索引缓冲。
 */
class SpriteBatch {
public:
    static std::unique_ptr<SpriteBatch> Create(me::rhi::GpuDevice& device);
    ~SpriteBatch();

    void Begin(const me::Matrix4x4& viewProj);
    void Submit(const SpriteDesc& sprite);
    void End(ID3D12GraphicsCommandList* cmd);

    /// @brief 上次 End 发出的 drawcall 数(同纹理 run 合为一次)。
    size_t DrawCallCount() const { return m_drawCallCount; }

private:
    SpriteBatch() = default;
    bool EnsureCapacity(ID3D12Device* device, uint32_t spriteCount); // 不足则增长

    me::rhi::ComPtr<ID3D12RootSignature> m_rootSig;
    me::rhi::ComPtr<ID3D12PipelineState> m_pso;
    std::unique_ptr<me::rhi::GpuBuffer> m_vb;  // 动态,持久映射
    std::unique_ptr<me::rhi::GpuBuffer> m_ib;  // 静态四边形索引(位置型,R32)
    D3D12_INDEX_BUFFER_VIEW m_ibv{};
    me::rhi::ComPtr<ID3D12Device> m_device;    // 用于按需增长缓冲
    uint32_t m_capacity = 0;                   // 当前缓冲可容纳的精灵数

    me::Matrix4x4 m_viewProj{};
    std::vector<SpriteDesc> m_sprites;
    bool m_inFrame = false;
    size_t m_drawCallCount = 0;
};

} // namespace me::render
```

- [ ] **Step 4: 写 SpriteBatch.cpp**

新建 `engine/renderer/src/SpriteBatch.cpp`:

```cpp
#include "me/render/SpriteBatch.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "me/rhi/GpuDevice.h"
#include "me/rhi/GpuBuffer.h"
#include "me/rhi/GpuTexture.h"
#include "me/rhi/Shader.h"
#include "me/core/Assert.h"
#include "me/core/Log.h"

namespace me::render {

namespace {
using namespace me::rhi;

constexpr uint32_t kViewProjConstantCount = 16; // 4x4 float
constexpr uint32_t kRootParamViewProj = 0;
constexpr uint32_t kRootParamTexture = 1;
constexpr uint32_t kVertsPerSprite = 4;
constexpr uint32_t kIndicesPerSprite = 6;

/// 顶点:世界像素坐标 + UV + RGBA 色调。布局须与 sprite.hlsl 输入一致。
struct BatchVertex {
    float x, y;
    float u, v;
    float r, g, b, a;
};

std::wstring ShaderPath() {
    const std::string p = std::string(ME_ASSET_DIR) + "/shaders/sprite.hlsl";
    return std::wstring(p.begin(), p.end());
}

/// 为 spriteCount 个精灵生成位置型四边形索引:精灵 i 引用顶点 i*4 + {0,1,2,0,2,3}。
std::vector<uint32_t> BuildQuadIndices(uint32_t spriteCount) {
    std::vector<uint32_t> indices(static_cast<size_t>(spriteCount) * kIndicesPerSprite);
    for (uint32_t i = 0; i < spriteCount; ++i) {
        const uint32_t base = i * kVertsPerSprite;
        uint32_t* dst = &indices[static_cast<size_t>(i) * kIndicesPerSprite];
        dst[0] = base + 0; dst[1] = base + 1; dst[2] = base + 2;
        dst[3] = base + 0; dst[4] = base + 2; dst[5] = base + 3;
    }
    return indices;
}

/// 把一个 SpriteDesc 烘成 4 个世界空间顶点(含旋转、UV、色调)。
void BuildQuad(const SpriteDesc& s, BatchVertex out[kVertsPerSprite]) {
    const float hw = s.dstRect.width * 0.5f;
    const float hh = s.dstRect.height * 0.5f;
    const float cx = s.dstRect.x + hw;
    const float cy = s.dstRect.y + hh;
    const float cs = std::cos(s.rotation);
    const float sn = std::sin(s.rotation);
    // 局部角点 (lx,ly) 绕中心旋转(行向量:[lx,ly]*R = (lx*c - ly*s, lx*s + ly*c))。
    auto place = [&](float lx, float ly, float& ox, float& oy) {
        ox = cx + (lx * cs - ly * sn);
        oy = cy + (lx * sn + ly * cs);
    };
    const float uMin = s.srcRect.x;
    const float uMax = s.srcRect.x + s.srcRect.width;
    const float vMin = s.srcRect.y;                  // 上
    const float vMax = s.srcRect.y + s.srcRect.height; // 下
    const me::Vector4& c = s.color;
    // 世界 Y 向上、纹理 V 向下:底边(-hh)取 vMax,顶边(+hh)取 vMin。
    struct Corner { float lx, ly, u, v; };
    const Corner corners[kVertsPerSprite] = {
        {-hw, -hh, uMin, vMax}, // 0 左下
        { hw, -hh, uMax, vMax}, // 1 右下
        { hw,  hh, uMax, vMin}, // 2 右上
        {-hw,  hh, uMin, vMin}, // 3 左上
    };
    for (uint32_t i = 0; i < kVertsPerSprite; ++i) {
        place(corners[i].lx, corners[i].ly, out[i].x, out[i].y);
        out[i].u = corners[i].u;
        out[i].v = corners[i].v;
        out[i].r = c.x; out[i].g = c.y; out[i].b = c.z; out[i].a = c.w;
    }
}
} // namespace

std::unique_ptr<SpriteBatch> SpriteBatch::Create(GpuDevice& device) {
    auto self = std::unique_ptr<SpriteBatch>(new SpriteBatch());
    ID3D12Device* dev = device.Device();
    self->m_device = dev;

    // 1) 着色器(FXC SM5.1)。
    ComPtr<ID3DBlob> vs = CompileHlsl(ShaderPath(), "VSMain", "vs_5_1");
    ComPtr<ID3DBlob> ps = CompileHlsl(ShaderPath(), "PSMain", "ps_5_1");
    if (!vs || !ps) return nullptr;

    // 2) 根签名:b0 = 16 根常量(viewProj);t0 = SRV 表;s0 = 静态采样器。
    D3D12_DESCRIPTOR_RANGE srvRange = {};
    srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRange.NumDescriptors = 1;
    srvRange.BaseShaderRegister = 0; // t0

    D3D12_ROOT_PARAMETER params[2] = {};
    params[kRootParamViewProj].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    params[kRootParamViewProj].Constants.Num32BitValues = kViewProjConstantCount;
    params[kRootParamViewProj].Constants.ShaderRegister = 0; // b0
    params[kRootParamViewProj].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    params[kRootParamTexture].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
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

    // 3) 输入布局 + PSO(POSITION/TEXCOORD/COLOR)。
    D3D12_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso = {};
    pso.pRootSignature = self->m_rootSig.Get();
    pso.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
    pso.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
    pso.InputLayout = {layout, 3};
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.NumRenderTargets = 1;
    pso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pso.SampleDesc.Count = 1;
    pso.SampleMask = UINT_MAX;
    pso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pso.RasterizerState.DepthClipEnable = TRUE;
    for (auto& rt : pso.BlendState.RenderTarget) {
        rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    }
    pso.DepthStencilState.DepthEnable = FALSE;
    pso.DepthStencilState.StencilEnable = FALSE;

    if (FAILED(dev->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&self->m_pso)))) {
        return nullptr;
    }

    // 4) 初始动态 VB + 静态 IB(容量 kInitialSpriteCapacity)。
    if (!self->EnsureCapacity(dev, kInitialSpriteCapacity)) return nullptr;
    return self;
}

SpriteBatch::~SpriteBatch() = default;

bool SpriteBatch::EnsureCapacity(ID3D12Device* device, uint32_t spriteCount) {
    if (spriteCount <= m_capacity) return true;
    // 增长到请求容量(高水位,不回缩)。
    m_vb = GpuBuffer::CreateDynamic(
        device, static_cast<size_t>(spriteCount) * kVertsPerSprite * sizeof(BatchVertex));
    const std::vector<uint32_t> indices = BuildQuadIndices(spriteCount);
    m_ib = GpuBuffer::CreateUpload(device, indices.data(),
                                   indices.size() * sizeof(uint32_t));
    if (!m_vb || !m_ib) {
        ME_LOG_ERROR("SpriteBatch: 顶点/索引缓冲增长失败");
        m_capacity = 0;
        return false;
    }
    m_ibv.BufferLocation = m_ib->Gpu();
    m_ibv.SizeInBytes = static_cast<UINT>(m_ib->Size());
    m_ibv.Format = DXGI_FORMAT_R32_UINT;
    m_capacity = spriteCount;
    return true;
}

void SpriteBatch::Begin(const me::Matrix4x4& viewProj) {
    ME_ASSERT_MSG(!m_inFrame, "SpriteBatch::Begin: 未配对的 Begin/End");
    m_inFrame = true;
    m_viewProj = viewProj;
    m_sprites.clear();
    m_drawCallCount = 0;
}

void SpriteBatch::Submit(const SpriteDesc& sprite) {
    ME_ASSERT_MSG(m_inFrame, "SpriteBatch::Submit: 必须在 Begin 与 End 之间调用");
    if (sprite.texture == nullptr) {
        ME_LOG_WARN("SpriteBatch::Submit: 跳过纹理为空的精灵");
        return;
    }
    m_sprites.push_back(sprite);
}

void SpriteBatch::End(ID3D12GraphicsCommandList* cmd) {
    ME_ASSERT_MSG(m_inFrame, "SpriteBatch::End: 未配对的 Begin/End");
    m_inFrame = false;
    m_drawCallCount = 0;
    if (m_sprites.empty()) return;

    if (!EnsureCapacity(m_device.Get(), static_cast<uint32_t>(m_sprites.size()))) return;

    // 按纹理指针稳定排序:同纹理相邻 → 合为一次 drawcall。
    std::stable_sort(m_sprites.begin(), m_sprites.end(),
                     [](const SpriteDesc& a, const SpriteDesc& b) {
                         return a.texture < b.texture;
                     });

    // 烘顶点并写入动态 VB。
    std::vector<BatchVertex> verts(m_sprites.size() * kVertsPerSprite);
    for (size_t i = 0; i < m_sprites.size(); ++i) {
        BuildQuad(m_sprites[i], &verts[i * kVertsPerSprite]);
    }
    const size_t vbBytes = verts.size() * sizeof(BatchVertex);
    m_vb->Write(verts.data(), vbBytes, 0);

    // 公共绑定。
    D3D12_VERTEX_BUFFER_VIEW vbv = {};
    vbv.BufferLocation = m_vb->Gpu();
    vbv.SizeInBytes = static_cast<UINT>(vbBytes);
    vbv.StrideInBytes = sizeof(BatchVertex);

    cmd->SetPipelineState(m_pso.Get());
    cmd->SetGraphicsRootSignature(m_rootSig.Get());
    cmd->SetGraphicsRoot32BitConstants(kRootParamViewProj, kViewProjConstantCount,
                                       &m_viewProj, 0);
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmd->IASetVertexBuffers(0, 1, &vbv);
    cmd->IASetIndexBuffer(&m_ibv);

    // 逐纹理 run:[runStart, runEnd) 同纹理 → 一次 DrawIndexedInstanced。
    size_t runStart = 0;
    while (runStart < m_sprites.size()) {
        const me::rhi::GpuTexture* tex = m_sprites[runStart].texture;
        size_t runEnd = runStart;
        while (runEnd < m_sprites.size() && m_sprites[runEnd].texture == tex) ++runEnd;
        const UINT count = static_cast<UINT>(runEnd - runStart);
        cmd->SetGraphicsRootDescriptorTable(kRootParamTexture, tex->SrvGpu());
        // 位置型索引:StartIndexLocation 已指向 runStart*4 起的顶点,BaseVertex=0。
        cmd->DrawIndexedInstanced(count * kIndicesPerSprite, 1,
                                  static_cast<UINT>(runStart) * kIndicesPerSprite, 0, 0);
        ++m_drawCallCount;
        runStart = runEnd;
    }
}

} // namespace me::render
```

- [ ] **Step 5: 删除 SpriteRenderer 并更新 renderer CMake**

```bash
git rm engine/renderer/include/me/render/SpriteRenderer.h \
       engine/renderer/src/SpriteRenderer.cpp
```

编辑 `engine/renderer/CMakeLists.txt`,把 `src/SpriteRenderer.cpp` 改为 `src/SpriteBatch.cpp`:

```cmake
if(WIN32)
    add_library(me_renderer STATIC
        src/SpriteBatch.cpp
    )
    target_include_directories(me_renderer PUBLIC include)
    target_compile_features(me_renderer PUBLIC cxx_std_17)
    # 单向依赖:renderer → core, rhi(含 me_rhi_cpu), assets。
    target_link_libraries(me_renderer PUBLIC me_core me_rhi me_rhi_cpu me_assets)
endif()
```

- [ ] **Step 6: 把 GPU 单精灵测试改用 SpriteBatch(含色调)**

把 `tests/gpu/test_sprite_render.cpp` 整文件替换为:

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
#include "me/core/Vector4.h"
#include "me/core/Rect.h"
#include "me/render/SpriteBatch.h"
#include "me/render/SpriteDesc.h"

using namespace me::rhi;
using me::render::SpriteBatch;
using me::render::SpriteDesc;

namespace {
constexpr uint32_t kRt = 8; // 8x8 离屏目标

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

TEST_CASE("SpriteBatch 单精灵:中心=贴图色,四角=清屏色") {
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

    const uint8_t red[4] = {255, 0, 0, 255};
    auto srv = srvHeap->Allocate();
    auto tex = GpuTexture::Create(device->Device(), device->Queue(), *fence,
                                  1, 1, red, srv);
    REQUIRE(tex != nullptr);

    auto batch = SpriteBatch::Create(*device);
    REQUIRE(batch != nullptr);

    // viewProj=8x8 像素正交;dstRect 覆盖中心 4x4(world [2,6]² → NDC [-0.5,0.5]²)。
    const me::Matrix4x4 vp = me::Matrix4x4::Orthographic(0, kRt, 0, kRt, 0, 1);
    SpriteDesc d;
    d.texture = tex.get();
    d.dstRect = me::Rect{2.0f, 2.0f, 4.0f, 4.0f};

    auto* cmd = ctx->Begin();
    D3D12_VIEWPORT vpRect = {0, 0, float(kRt), float(kRt), 0.0f, 1.0f};
    D3D12_RECT sc = {0, 0, LONG(kRt), LONG(kRt)};
    cmd->RSSetViewports(1, &vpRect);
    cmd->RSSetScissorRects(1, &sc);
    cmd->OMSetRenderTargets(1, &rtvDesc.cpu, FALSE, nullptr);
    const float blue[4] = {0.0f, 0.0f, 1.0f, 1.0f};
    cmd->ClearRenderTargetView(rtvDesc.cpu, blue, 0, nullptr);
    ID3D12DescriptorHeap* heaps[] = {srvHeap->Heap()};
    cmd->SetDescriptorHeaps(1, heaps);

    batch->Begin(vp);
    batch->Submit(d);
    batch->End(cmd);

    ctx->End();
    ctx->Execute(device->Queue());
    fence->Flush(device->Queue());

    auto px = ReadbackRgba8(device->Device(), device->Queue(), *fence,
                            rt.Get(), kRt, kRt, D3D12_RESOURCE_STATE_RENDER_TARGET);
    auto at = [&](uint32_t x, uint32_t y) { return &px[(y * kRt + x) * 4]; };

    CHECK(at(kRt/2, kRt/2)[0] > 200); // 中心红
    CHECK(at(kRt/2, kRt/2)[2] < 60);
    CHECK(at(0, 0)[2] > 200);         // 角蓝(清屏)
    CHECK(at(0, 0)[0] < 60);
    CHECK(batch->DrawCallCount() == 1);
}

TEST_CASE("SpriteBatch 色调:白贴图 × 绿色调 = 绿") {
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

    const uint8_t white[4] = {255, 255, 255, 255};
    auto srv = srvHeap->Allocate();
    auto tex = GpuTexture::Create(device->Device(), device->Queue(), *fence,
                                  1, 1, white, srv);
    REQUIRE(tex != nullptr);

    auto batch = SpriteBatch::Create(*device);
    REQUIRE(batch != nullptr);

    const me::Matrix4x4 vp = me::Matrix4x4::Orthographic(0, kRt, 0, kRt, 0, 1);
    SpriteDesc d;
    d.texture = tex.get();
    d.dstRect = me::Rect{0.0f, 0.0f, float(kRt), float(kRt)}; // 铺满
    d.color = me::Vector4{0.0f, 1.0f, 0.0f, 1.0f};            // 绿色调

    auto* cmd = ctx->Begin();
    D3D12_VIEWPORT vpRect = {0, 0, float(kRt), float(kRt), 0.0f, 1.0f};
    D3D12_RECT sc = {0, 0, LONG(kRt), LONG(kRt)};
    cmd->RSSetViewports(1, &vpRect);
    cmd->RSSetScissorRects(1, &sc);
    cmd->OMSetRenderTargets(1, &rtvDesc.cpu, FALSE, nullptr);
    const float black[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    cmd->ClearRenderTargetView(rtvDesc.cpu, black, 0, nullptr);
    ID3D12DescriptorHeap* heaps[] = {srvHeap->Heap()};
    cmd->SetDescriptorHeaps(1, heaps);

    batch->Begin(vp);
    batch->Submit(d);
    batch->End(cmd);

    ctx->End();
    ctx->Execute(device->Queue());
    fence->Flush(device->Queue());

    auto px = ReadbackRgba8(device->Device(), device->Queue(), *fence,
                            rt.Get(), kRt, kRt, D3D12_RESOURCE_STATE_RENDER_TARGET);
    auto at = [&](uint32_t x, uint32_t y) { return &px[(y * kRt + x) * 4]; };
    CHECK(at(kRt/2, kRt/2)[0] < 60);  // R 低
    CHECK(at(kRt/2, kRt/2)[1] > 200); // G 高
    CHECK(at(kRt/2, kRt/2)[2] < 60);  // B 低
}
```

- [ ] **Step 7: 构建并运行 GPU 测试,确认通过**

Run(Windows):
```powershell
cmake --build build --config Debug --target me_gpu_tests
ctest --test-dir build -C Debug -R me_gpu_tests --output-on-failure
```
Expected: PASS —— 含「SpriteBatch 单精灵…」「SpriteBatch 色调…」以及既有 device/fence、纹理往返测试。

- [ ] **Step 8: Commit**

```bash
git add engine/renderer/include/me/render/SpriteDesc.h \
        engine/renderer/include/me/render/SpriteBatch.h \
        engine/renderer/src/SpriteBatch.cpp \
        engine/renderer/CMakeLists.txt assets/shaders/sprite.hlsl \
        tests/gpu/test_sprite_render.cpp
git add -A engine/renderer/include/me/render/SpriteRenderer.h \
           engine/renderer/src/SpriteRenderer.cpp
git commit -m "feat(renderer): SpriteBatch 合批渲染器替换 M1 SpriteRenderer

- SpriteDesc(texture/srcRect/dstRect/color/rotation)+ 按纹理稳定排序合批
- 顶点 pos+uv+color,模型变换 CPU 端烘入;着色器 uMVP→uViewProj + 色调相乘
- 退役单精灵 SpriteRenderer;GPU 测试改用 SpriteBatch + 色调用例

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 4: 多精灵合批语义(分组 / srcRect / 多纹理 / 容量增长)

**Files:**
- Create: `tests/gpu/test_sprite_batch.cpp`
- Modify: `engine/rhi/CMakeLists.txt`(`me_gpu_tests` 源列表加 `test_sprite_batch.cpp`)

**Interfaces:**
- Consumes: T3 的 `SpriteBatch`(`Begin/Submit/End/DrawCallCount`)、`SpriteDesc`、`kInitialSpriteCapacity`、RHI 测试设施(同 `test_sprite_render.cpp`)。
- Produces: 无新生产接口;纯验证 T3 的合批分组、srcRect 采样、多纹理、容量增长行为。

- [ ] **Step 1: 注册测试文件,写失败测试**

编辑 `engine/rhi/CMakeLists.txt`,在 `add_executable(me_gpu_tests ...)` 源列表中(`test_sprite_render.cpp` 之后)追加:

```cmake
            ${CMAKE_SOURCE_DIR}/tests/gpu/test_sprite_batch.cpp
```

新建 `tests/gpu/test_sprite_batch.cpp`:

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
#include "me/core/Vector4.h"
#include "me/core/Rect.h"
#include "me/render/SpriteBatch.h"
#include "me/render/SpriteDesc.h"

using namespace me::rhi;
using me::render::SpriteBatch;
using me::render::SpriteDesc;

namespace {
constexpr uint32_t kRt = 8;

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

// 1x1 纯色纹理 helper(各自占一个 SRV)。
std::unique_ptr<GpuTexture> MakeSolidTexture(GpuDevice& device, Fence& fence,
                                             DescriptorHeap& srvHeap,
                                             uint8_t r, uint8_t g, uint8_t b) {
    const uint8_t px[4] = {r, g, b, 255};
    auto srv = srvHeap.Allocate();
    return GpuTexture::Create(device.Device(), device.Queue(), fence, 1, 1, px, srv);
}
} // namespace

TEST_CASE("SpriteBatch 分组:同纹理合 1 次,两纹理 2 次,与提交顺序无关") {
    auto device = GpuDevice::Create(true);
    REQUIRE(device != nullptr);
    auto fence = Fence::Create(device->Device());
    auto ctx = CommandContext::Create(device->Device());
    auto rtvHeap = DescriptorHeap::Create(device->Device(),
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1, false);
    auto srvHeap = DescriptorHeap::Create(device->Device(),
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 4, true);
    REQUIRE(fence); REQUIRE(ctx); REQUIRE(rtvHeap); REQUIRE(srvHeap);
    auto rtvDesc = rtvHeap->Allocate();
    auto rt = MakeRenderTarget(device->Device(), rtvDesc.cpu);

    auto texA = MakeSolidTexture(*device, *fence, *srvHeap, 255, 0, 0);
    auto texB = MakeSolidTexture(*device, *fence, *srvHeap, 0, 255, 0);
    REQUIRE(texA != nullptr); REQUIRE(texB != nullptr);
    auto batch = SpriteBatch::Create(*device);
    REQUIRE(batch != nullptr);

    const me::Matrix4x4 vp = me::Matrix4x4::Orthographic(0, kRt, 0, kRt, 0, 1);
    auto sprite = [](const GpuTexture* t, float x) {
        SpriteDesc d; d.texture = t; d.dstRect = me::Rect{x, 0.0f, 4.0f, 8.0f}; return d;
    };

    auto* cmd = ctx->Begin();
    D3D12_VIEWPORT vpRect = {0, 0, float(kRt), float(kRt), 0.0f, 1.0f};
    D3D12_RECT sc = {0, 0, LONG(kRt), LONG(kRt)};
    cmd->RSSetViewports(1, &vpRect);
    cmd->RSSetScissorRects(1, &sc);
    cmd->OMSetRenderTargets(1, &rtvDesc.cpu, FALSE, nullptr);
    const float black[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    cmd->ClearRenderTargetView(rtvDesc.cpu, black, 0, nullptr);
    ID3D12DescriptorHeap* heaps[] = {srvHeap->Heap()};
    cmd->SetDescriptorHeaps(1, heaps);

    // 交错提交 A,B,A:稳定排序后 [A,A,B] → 2 次 drawcall。
    batch->Begin(vp);
    batch->Submit(sprite(texA.get(), 0.0f));
    batch->Submit(sprite(texB.get(), 4.0f));
    batch->Submit(sprite(texA.get(), 0.0f));
    batch->End(cmd);

    ctx->End();
    ctx->Execute(device->Queue());
    fence->Flush(device->Queue());

    CHECK(batch->DrawCallCount() == 2);

    auto px = ReadbackRgba8(device->Device(), device->Queue(), *fence,
                            rt.Get(), kRt, kRt, D3D12_RESOURCE_STATE_RENDER_TARGET);
    auto at = [&](uint32_t x, uint32_t y) { return &px[(y * kRt + x) * 4]; };
    CHECK(at(2, 4)[0] > 200); // 左半红(texA)
    CHECK(at(6, 4)[1] > 200); // 右半绿(texB)
}

TEST_CASE("SpriteBatch srcRect:从 2x2 纹理取左上角 texel") {
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

    // 2x2:左上红、右上绿、左下蓝、右下白(行主序,row0 = 上)。
    const uint8_t pixels[16] = {
        255, 0, 0, 255,   0, 255, 0, 255,   // 上行:红 绿
        0, 0, 255, 255,   255, 255, 255, 255 // 下行:蓝 白
    };
    auto srv = srvHeap->Allocate();
    auto tex = GpuTexture::Create(device->Device(), device->Queue(), *fence,
                                  2, 2, pixels, srv);
    REQUIRE(tex != nullptr);
    auto batch = SpriteBatch::Create(*device);
    REQUIRE(batch != nullptr);

    const me::Matrix4x4 vp = me::Matrix4x4::Orthographic(0, kRt, 0, kRt, 0, 1);
    SpriteDesc d;
    d.texture = tex.get();
    d.dstRect = me::Rect{0.0f, 0.0f, float(kRt), float(kRt)};
    d.srcRect = me::Rect{0.0f, 0.0f, 0.5f, 0.5f}; // 左上 texel(UV 原点左上,V 向下)

    auto* cmd = ctx->Begin();
    D3D12_VIEWPORT vpRect = {0, 0, float(kRt), float(kRt), 0.0f, 1.0f};
    D3D12_RECT sc = {0, 0, LONG(kRt), LONG(kRt)};
    cmd->RSSetViewports(1, &vpRect);
    cmd->RSSetScissorRects(1, &sc);
    cmd->OMSetRenderTargets(1, &rtvDesc.cpu, FALSE, nullptr);
    const float black[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    cmd->ClearRenderTargetView(rtvDesc.cpu, black, 0, nullptr);
    ID3D12DescriptorHeap* heaps[] = {srvHeap->Heap()};
    cmd->SetDescriptorHeaps(1, heaps);

    batch->Begin(vp);
    batch->Submit(d);
    batch->End(cmd);

    ctx->End();
    ctx->Execute(device->Queue());
    fence->Flush(device->Queue());

    auto px = ReadbackRgba8(device->Device(), device->Queue(), *fence,
                            rt.Get(), kRt, kRt, D3D12_RESOURCE_STATE_RENDER_TARGET);
    auto at = [&](uint32_t x, uint32_t y) { return &px[(y * kRt + x) * 4]; };
    // 整个 dst 只采样左上 texel → 处处为红。
    CHECK(at(kRt/2, kRt/2)[0] > 200);
    CHECK(at(kRt/2, kRt/2)[1] < 60);
    CHECK(at(kRt/2, kRt/2)[2] < 60);
}

TEST_CASE("SpriteBatch 容量增长:提交超过初始容量仍单纹理合 1 次") {
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

    auto tex = MakeSolidTexture(*device, *fence, *srvHeap, 255, 0, 0);
    REQUIRE(tex != nullptr);
    auto batch = SpriteBatch::Create(*device);
    REQUIRE(batch != nullptr);

    const me::Matrix4x4 vp = me::Matrix4x4::Orthographic(0, kRt, 0, kRt, 0, 1);
    auto* cmd = ctx->Begin();
    D3D12_VIEWPORT vpRect = {0, 0, float(kRt), float(kRt), 0.0f, 1.0f};
    D3D12_RECT sc = {0, 0, LONG(kRt), LONG(kRt)};
    cmd->RSSetViewports(1, &vpRect);
    cmd->RSSetScissorRects(1, &sc);
    cmd->OMSetRenderTargets(1, &rtvDesc.cpu, FALSE, nullptr);
    const float black[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    cmd->ClearRenderTargetView(rtvDesc.cpu, black, 0, nullptr);
    ID3D12DescriptorHeap* heaps[] = {srvHeap->Heap()};
    cmd->SetDescriptorHeaps(1, heaps);

    batch->Begin(vp);
    // 超过初始容量 → End 内 EnsureCapacity 增长缓冲,不丢精灵。
    const uint32_t n = me::render::kInitialSpriteCapacity + 1;
    SpriteDesc d; d.texture = tex.get(); d.dstRect = me::Rect{2.0f, 2.0f, 4.0f, 4.0f};
    for (uint32_t i = 0; i < n; ++i) batch->Submit(d);
    batch->End(cmd);

    ctx->End();
    ctx->Execute(device->Queue());
    fence->Flush(device->Queue());

    CHECK(batch->DrawCallCount() == 1); // 同纹理 → 仍 1 次
    auto px = ReadbackRgba8(device->Device(), device->Queue(), *fence,
                            rt.Get(), kRt, kRt, D3D12_RESOURCE_STATE_RENDER_TARGET);
    auto at = [&](uint32_t x, uint32_t y) { return &px[(y * kRt + x) * 4]; };
    CHECK(at(kRt/2, kRt/2)[0] > 200); // 中心红
}
```

- [ ] **Step 2: 构建并确认失败(测试逻辑或行为缺陷暴露前先验证编译/链接)**

Run(Windows):
```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DME_BUILD_TESTS=ON -DME_BUILD_GPU_TESTS=ON
cmake --build build --config Debug --target me_gpu_tests
```
Expected: 编译链接通过(`SpriteBatch` 已在 T3 提供);若任何断言不符则在 Step 3 运行时暴露。

- [ ] **Step 3: 运行 GPU 测试,确认全绿**

Run:
```powershell
ctest --test-dir build -C Debug -R me_gpu_tests --output-on-failure
```
Expected: PASS —— 分组(DrawCallCount==2 + 左红右绿)、srcRect(处处红)、容量增长(DrawCallCount==1 + 中心红)三个新用例全过。

> 若分组用例得到 3 而非 2:确认 `End` 内已按 `texture` 指针 `stable_sort` 且逐 run 计数。若 srcRect 用例非红:核对 `BuildQuad` 的 v 轴映射(底边 vMax、顶边 vMin)与 `srcRect` 字段语义。

- [ ] **Step 4: Commit**

```bash
git add tests/gpu/test_sprite_batch.cpp engine/rhi/CMakeLists.txt
git commit -m "test(renderer): SpriteBatch 合批分组/srcRect/多纹理/容量增长 GPU 测试

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 5: sandbox 改为多精灵 + 可控相机

**Files:**
- Modify: `engine/platform/include/me/platform/Input.h`(`KeyCode` 加 `Q`/`E`)
- Modify: `engine/platform/src/Input_Win32.cpp`(Win32 键映射加 `Q`/`E`)
- Modify: `sandbox/main.cpp`

**Interfaces:**
- Consumes: T2 `OrthographicCamera`、T3 `SpriteBatch`/`SpriteDesc`、现有 `Window`/`InputState`/`KeyCode`、RHI 设施。
- Produces: `me::platform::KeyCode::Q`、`me::platform::KeyCode::E`(供缩放输入)。手动目视验证(本里程碑无自动化测试覆盖 sandbox)。

- [ ] **Step 1: 给 KeyCode 增加 Q/E 并补 Win32 映射**

现有 `KeyCode` 仅有 `Escape, Space, W, A, S, D, Count`,缩放需要新增 `Q`/`E`。

编辑 `engine/platform/include/me/platform/Input.h:10`,把枚举改为:

```cpp
enum class KeyCode { Escape, Space, W, A, S, D, Q, E, Count };
```

编辑 `engine/platform/src/Input_Win32.cpp`,在 `MapPlatformKey` 的 `case 'D'` 之后(第 14 行后)追加:

```cpp
    case 'Q':       return KeyCode::Q;
    case 'E':       return KeyCode::E;
```

- [ ] **Step 2: 重写 sandbox/main.cpp**

把 `sandbox/main.cpp` 整文件替换为(在 M1 基础上:换 `SpriteBatch` + `OrthographicCamera`,铺多个精灵,改输入为相机平移/缩放):

```cpp
#include <memory>
#include <vector>

#include "me/platform/Window.h"
#include "me/platform/Input.h"
#include "me/assets/ImageData.h"
#include "me/core/Log.h"
#include "me/core/Matrix4x4.h"
#include "me/core/Vector2.h"
#include "me/core/Vector4.h"
#include "me/core/Rect.h"

#include "me/rhi/GpuDevice.h"
#include "me/rhi/SwapChain.h"
#include "me/rhi/Fence.h"
#include "me/rhi/CommandContext.h"
#include "me/rhi/DescriptorHeap.h"
#include "me/rhi/GpuTexture.h"
#include "me/render/SpriteBatch.h"
#include "me/render/SpriteDesc.h"
#include "me/render/OrthographicCamera.h"

#include <d3d12.h>

using namespace me;

namespace {
constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr float kSpritePixels = 64.0f;   // 精灵边长(像素)
constexpr float kGridCols = 8.0f;        // 精灵网格列数
constexpr float kGridRows = 5.0f;        // 精灵网格行数
constexpr float kGridSpacing = 96.0f;    // 精灵间距(像素)
constexpr float kCameraSpeed = 6.0f;     // 每帧相机平移步长(像素)
constexpr float kZoomStep = 0.02f;       // 每帧缩放步长
constexpr float kMinZoom = 0.25f;
constexpr float kMaxZoom = 4.0f;

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

float Clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
} // namespace

int main() {
    platform::WindowDesc wd;
    wd.width = kWindowWidth;
    wd.height = kWindowHeight;
    wd.title = "MiniEngine M2 — SpriteBatch + Camera";
    auto window = platform::Window::Create(wd);
    if (!window) { ME_LOG_ERROR("窗口创建失败"); return 1; }

    platform::InputState input;
    window->SetInput(&input);

    auto device = rhi::GpuDevice::Create(/*useWarp=*/false);
    if (!device) device = rhi::GpuDevice::Create(/*useWarp=*/true);
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
    auto batch = render::SpriteBatch::Create(*device);
    if (!texture || !batch) { ME_LOG_ERROR("纹理/批渲染器创建失败"); return 1; }

    // 相机居中于精灵网格中心。
    render::OrthographicCamera camera;
    camera.SetViewportSize(float(kWindowWidth), float(kWindowHeight));
    camera.SetPosition(Vector2{kGridCols * kGridSpacing * 0.5f,
                               kGridRows * kGridSpacing * 0.5f});
    camera.SetZoom(1.0f);

    while (!window->ShouldClose()) {
        input.NewFrame();
        window->PumpMessages();
        if (input.WasPressed(platform::KeyCode::Escape)) break;

        // 输入驱动相机:WASD 平移、Q/E 缩放。
        Vector2 camPos = camera.Position();
        if (input.IsDown(platform::KeyCode::A)) camPos.x -= kCameraSpeed;
        if (input.IsDown(platform::KeyCode::D)) camPos.x += kCameraSpeed;
        if (input.IsDown(platform::KeyCode::W)) camPos.y += kCameraSpeed;
        if (input.IsDown(platform::KeyCode::S)) camPos.y -= kCameraSpeed;
        camera.SetPosition(camPos);
        float zoom = camera.Zoom();
        if (input.IsDown(platform::KeyCode::Q)) zoom -= kZoomStep;
        if (input.IsDown(platform::KeyCode::E)) zoom += kZoomStep;
        camera.SetZoom(Clamp(zoom, kMinZoom, kMaxZoom));

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
        const float clearColor[4] = {0.10f, 0.12f, 0.16f, 1.0f};
        cmd->ClearRenderTargetView(rtv, clearColor, 0, nullptr);

        ID3D12DescriptorHeap* heaps[] = {srvHeap->Heap()};
        cmd->SetDescriptorHeaps(1, heaps);

        // 铺一片静态精灵网格,带轻微色调梯度,验证合批。
        batch->Begin(camera.ViewProj());
        for (int row = 0; row < int(kGridRows); ++row) {
            for (int col = 0; col < int(kGridCols); ++col) {
                render::SpriteDesc d;
                d.texture = texture.get();
                d.dstRect = Rect{col * kGridSpacing, row * kGridSpacing,
                                 kSpritePixels, kSpritePixels};
                const float t = float(col) / kGridCols;
                d.color = Vector4{1.0f, 1.0f - 0.5f * t, 1.0f - 0.5f * t, 1.0f};
                batch->Submit(d);
            }
        }
        batch->End(cmd);

        Transition(cmd, back, D3D12_RESOURCE_STATE_RENDER_TARGET,
                   D3D12_RESOURCE_STATE_PRESENT);
        ctx->End();
        ctx->Execute(device->Queue());
        swapChain->Present();
        fence->Flush(device->Queue()); // M2 仍每帧全同步(帧并行后续里程碑)
        ctx->AdvanceFrame();
    }

    fence->Flush(device->Queue());
    return 0;
}
```

- [ ] **Step 3: 构建 sandbox(Windows)**

Run:
```powershell
cmake --build build --config Debug --target sandbox
```
Expected: 链接通过,生成 `build\bin\Debug\sandbox.exe`。

- [ ] **Step 4: 目视验证**

Run:
```powershell
.\build\bin\Debug\sandbox.exe
```
Expected(人工确认):
- 看到一片精灵网格(约 8×5),贴图正立,从左到右有轻微色调梯度。
- 按 W/A/S/D 平移相机(画面整体反向移动),按 Q/E 缩放(精灵放大/缩小)。
- 按 ESC 正常退出,无报错、无崩溃。

- [ ] **Step 5: Commit**

```bash
git add sandbox/main.cpp engine/platform/include/me/platform/Input.h \
        engine/platform/src/Input_Win32.cpp
git commit -m "feat(sandbox): M2 多精灵合批 + 相机平移/缩放演示

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## Task 6: 文档回写(renderer README + PROGRESS + ADR)

**Files:**
- Modify: `engine/renderer/README.md`
- Modify: `docs/PROGRESS.md`

**Interfaces:** 无代码接口;仅文档。

- [ ] **Step 1: 更新 renderer README**

编辑 `engine/renderer/README.md`,反映:`SpriteBatch`(按纹理合批、`Begin/Submit/End`、`DrawCallCount`)、`OrthographicCamera`(header-only、position/zoom/viewport → viewProj)、`SpriteDesc`(提交单元、srcRect/dstRect/color/rotation 语义);移除 M1 `SpriteRenderer` 的描述。保持与既有 README 风格一致(模块职责 + 公开类型 + 依赖)。

- [ ] **Step 2: 更新 PROGRESS.md**

编辑 `docs/PROGRESS.md`:
- 顶部「最后更新」改为 `2026-06-19`;「当前阶段」改为「M2 批渲染 + 正交相机完成,下一步 M3 瓦片地图」。
- 里程碑表把 **M2** 状态由 ☐ 改为 ☑,说明补上「SpriteBatch 按纹理合批 + OrthographicCamera + 多精灵;WARP 多精灵/色调/srcRect 像素回读 + 相机 doctest + sandbox 目视」。
- 「一句话现状」追加 M2 完成摘要。
- 「下一步行动」改为对 **M3 瓦片地图** 走 `writing-plans`。
- 「关键决策记录」追加行(日期 2026-06-19):
  - 合批策略 = 按纹理指针稳定排序后逐 run 合 drawcall;模型变换 CPU 端烘入顶点(合批无法每精灵传根常量)。
  - 顶点格式 pos+uv+color 一次到位,srcRect 支持图集采样,供 M3 瓦片复用。
  - M1 `SpriteRenderer` 退役并入 `SpriteBatch`。
  - 延续上传堆 + 每帧全同步;帧并行(FrameRing)/默认堆迁移仍推迟到性能里程碑。
  - 容量溢出策略 = VB/IB 按高水位自动增长(不静默丢弃)。
- 「待解决 / 开放问题」:把「上传堆顶点…M2/性能里程碑改默认堆」与「M1 每帧 Flush…M2 引入 per-frame 围栏」两条更新为「仍延续,推迟到后续性能里程碑(M2 已确认不做帧并行)」。

- [ ] **Step 3: 最终全量验证**

Run(WSL,跨平台逻辑):
```bash
cmake --build build-wsl -j && ctest --test-dir build-wsl --output-on-failure
```
Run(Windows,GPU + sandbox 已在 T5 目视):
```powershell
cmake --build build --config Debug && ctest --test-dir build -C Debug --output-on-failure
```
Expected: `me_tests`(含相机用例)与 `me_gpu_tests`(含全部 SpriteBatch 用例)全绿。

- [ ] **Step 4: Commit**

```bash
git add engine/renderer/README.md docs/PROGRESS.md
git commit -m "docs(m2): 回写 M2 完成进度 + renderer README + ADR

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>"
```

---

## 完成定义(DoD)对照

- [ ] 相机单测在 WSL doctest 绿(T2)。
- [ ] `test_sprite_render` 改用 SpriteBatch + 色调,WARP 绿(T3)。
- [ ] 多精灵合批分组 / srcRect / 多纹理 / 容量增长 GPU 测试绿(T4)。
- [ ] sandbox 真机:多精灵合批显示、相机平移/缩放/退出可用、贴图正立(T5)。
- [ ] M1 `SpriteRenderer` 已删除且无悬挂引用;`CMakeLists.txt` / README 同步(T3、T6)。
- [ ] `docs/PROGRESS.md` 回写 M2 ☑ 及 ADR(T6)。
```
