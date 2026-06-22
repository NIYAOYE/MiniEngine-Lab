add_library(me_toolapi STATIC
    src/ToolResult.cpp
    src/SchemaValidator.cpp
    src/ToolInvocation.cpp
    src/ToolRegistry.cpp
    src/tools/QueryTools.cpp
    src/tools/MutationTools.cpp
    src/tools/BuiltinTools.cpp
    src/tools/TimeTools.cpp
)
target_include_directories(me_toolapi PUBLIC include)
target_compile_features(me_toolapi PUBLIC cxx_std_17)
# 单向依赖:toolapi → command → scene → core,以及 toolapi → domain(时间 Tool)。
target_link_libraries(me_toolapi PUBLIC me_command me_scene me_core me_domain nlohmann_json::nlohmann_json)
