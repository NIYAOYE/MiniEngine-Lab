# M0 唯一第三方依赖:doctest(单头单测框架)
include(FetchContent)

FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG        v2.4.11
)
FetchContent_MakeAvailable(doctest)
# 提供 target: doctest::doctest

# stb:单头库集合。仅用 stb_image(贴图解码)。
# stb 无版本 tag,故钉到一个具体 commit 以保证可复现构建(避免 master 漂移)。
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        31c1ad37456438565541f4919958214b6e762fb4
)
FetchContent_MakeAvailable(stb)

add_library(stb_image INTERFACE)
target_include_directories(stb_image INTERFACE ${stb_SOURCE_DIR})

# nlohmann/json:单头 JSON 库。用于 Tiled 地图解析(M3)、后续 Tool/序列化(M5+)。
# 用官方发布的 include.zip(仅 include/ 目录,约 100KB)而非 git clone 整库:
# 整库带完整测试数据与历史(数百 MB),FetchContent 全量克隆会卡很久。
# release 资源按 tag 不可变,省去 GIT clone 仍可复现。
FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/include.zip
)
FetchContent_MakeAvailable(nlohmann_json)

# 自建 INTERFACE target + 命名空间别名,沿用 stb_image 的单头封装方式。
add_library(nlohmann_json INTERFACE)
add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json)
target_include_directories(nlohmann_json INTERFACE ${nlohmann_json_SOURCE_DIR}/include)
# 提供 target: nlohmann_json::nlohmann_json

# Dear ImGui:调试 GUI / ToolAPI 客户端(M7)。仅 Windows 构建(DX12+Win32 后端)。
# 用 release tarball URL 而非 git clone,避免全量历史(沿用 nlohmann 教训)。
if(WIN32)
    FetchContent_Declare(
        imgui
        URL https://github.com/ocornut/imgui/archive/refs/tags/v1.91.5.tar.gz
    )
    FetchContent_MakeAvailable(imgui)

    add_library(imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_dx12.cpp
    )
    target_include_directories(imgui PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends)
    target_compile_features(imgui PUBLIC cxx_std_17)
    # DX12 后端所需系统库。
    target_link_libraries(imgui PUBLIC d3d12 dxgi)
endif()
