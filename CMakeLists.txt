cmake_minimum_required(VERSION 3.20)
project(MiniEngine LANGUAGES CXX)

# ---- 全局 C++ 标准(见计划 Global Constraints)----
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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
