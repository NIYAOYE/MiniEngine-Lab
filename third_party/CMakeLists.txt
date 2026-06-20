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
