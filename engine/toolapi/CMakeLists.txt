add_library(me_toolapi STATIC
    src/ToolResult.cpp
    src/SchemaValidator.cpp
    src/ToolInvocation.cpp
)
target_include_directories(me_toolapi PUBLIC include)
target_compile_features(me_toolapi PUBLIC cxx_std_17)
# 单向依赖:toolapi → command → scene → core。严禁依赖 rhi / renderer。
# nlohmann/json 是 Tool 参数/结果的边界格式,出现在公开头文件 → PUBLIC。
target_link_libraries(me_toolapi PUBLIC me_command me_scene me_core nlohmann_json::nlohmann_json)
