# MiniEngine

面向星露谷物语类 **2D/2.5D 农场模拟游戏**的轻量级 C++ 引擎(学习向,Agent-ready Tool API)。
设计文档见 `docs/superpowers/specs/`,进度见 `docs/PROGRESS.md`。

## 构建

需要 CMake ≥ 3.20 与 C++17 编译器。首次配置会用 git 拉取 doctest 与 stb。
DX12/Win32 代码无法在 WSL/Linux 编译,故分两套构建。

**跨平台逻辑(可在 WSL/Linux 跑)** —— Core / Platform(CPU)/ me_rhi_cpu / Assets:

```bash
cmake -S . -B build-wsl -DME_BUILD_TESTS=ON -DME_BUILD_GPU_TESTS=OFF
cmake --build build-wsl -j
ctest --test-dir build-wsl --output-on-failure
```

**Windows 构建(M1,DX12/GPU + 目视)** —— 需 Developer PowerShell / VS 2022 + Windows SDK:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DME_BUILD_TESTS=ON -DME_BUILD_GPU_TESTS=ON
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure   # 含 WARP 像素回读 GPU 测试
.\build\bin\Debug\sandbox.exe                          # 目视:精灵上屏 + WASD 平移
```

> **多配置生成器(Visual Studio)必须指定配置。** VS 在一个构建目录里同时容纳 Debug/Release,
> 所以构建要带 `--config Debug`、测试要带 `-C Debug`;两者缺一会报
> `Test not available without configuration. (Missing "-C <config>"?)`。
> 而 Linux/WSL 默认的 Makefile/Ninja 是单配置生成器(配置时即定死),无需 `-C`——
> 这也是上面"跨平台逻辑"一节的 `ctest` 不带 `-C` 的原因。

## 模块
- `engine/core`(`me_core`):2D 数学、句柄、日志、断言。
- `engine/platform`(`me_platform`):计时、文件系统;Win32 窗口/输入(仅 Windows)。
- `engine/rhi`(`me_rhi_cpu` / `me_rhi`):DX12 薄封装。`me_rhi_cpu` 为跨平台纯逻辑(同步/帧环/描述符/几何/精灵矩阵);`me_rhi` 为 Windows-only DX12 实现(Device/SwapChain/CommandContext/Fence/DescriptorHeap/GpuBuffer/GpuTexture/Shader/像素回读)。
- `engine/assets`(`me_assets`):图片解码(stb_image)→ RGBA8(跨平台)。
- `engine/renderer`(`me_renderer`,仅 Windows):SpriteRenderer(单精灵:根签名+PSO+单位四边形)。
- `sandbox`(仅 Windows):开窗 + DX12 上屏目视沙盒。
- `tests`(`me_tests` / `me_gpu_tests`):doctest 单元测试 / WARP GPU 像素回读测试。
