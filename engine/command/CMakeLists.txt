add_library(me_command STATIC
    src/CommandStack.cpp
    src/CreateEntityCmd.cpp
)
target_include_directories(me_command PUBLIC include)
target_compile_features(me_command PUBLIC cxx_std_17)
# 单向依赖:command → scene → core。严禁依赖 rhi / renderer。
target_link_libraries(me_command PUBLIC me_scene)
