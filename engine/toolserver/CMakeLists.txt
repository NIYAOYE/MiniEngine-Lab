add_library(me_toolserver STATIC
    src/ToolDispatcher.cpp
)
target_include_directories(me_toolserver PUBLIC include)
target_compile_features(me_toolserver PUBLIC cxx_std_17)
# 单向依赖:toolserver → toolapi → command → scene → core,以及 toolapi/domain。
# 它是应用层装配,不被任何引擎模块反向依赖。
target_link_libraries(me_toolserver PUBLIC
    me_toolapi me_domain me_scene me_command me_core nlohmann_json::nlohmann_json)
