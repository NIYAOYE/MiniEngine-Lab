cmake_minimum_required(VERSION 3.20)
project(MiniEngine LANGUAGES CXX)

# ---- 全局 C++ 标准(见计划 Global Constraints)----
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---- 选项 ----
option(ME_BUILD_TESTS "Build unit tests" ON)
option(ME_BUILD_GPU_TESTS "Build DX12/WARP GPU tests (Windows only)" OFF)

# MSVC 必须 /utf-8:源文件含中文注释,否则 GBK 误读会编译失败(见计划 Global Constraints)。
add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

# Windows 全程使用宽字符(-W)API:定义 UNICODE/_UNICODE,使 IDC_ARROW 等资源宏
# 解析为宽字符版本,与 LoadCursorW/CreateWindowExW 等一致。
if(WIN32)
    add_compile_definitions(UNICODE _UNICODE)
endif()

# 单一构建输出目录,便于查找产物
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 必须在添加任何注册测试的子目录(含 engine/rhi 的 me_gpu_tests)之前启用测试。
if(ME_BUILD_TESTS OR ME_BUILD_GPU_TESTS)
    enable_testing()
endif()

add_subdirectory(third_party)
add_subdirectory(engine/core)
add_subdirectory(engine/platform)
add_subdirectory(engine/rhi)
add_subdirectory(engine/assets)
add_subdirectory(engine/renderer)

if(ME_BUILD_TESTS)
    add_subdirectory(tests)
endif()
