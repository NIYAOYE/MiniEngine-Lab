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
