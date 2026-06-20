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
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)
# 提供 target: nlohmann_json::nlohmann_json
