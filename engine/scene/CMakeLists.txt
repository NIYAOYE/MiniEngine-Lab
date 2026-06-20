add_library(me_scene STATIC
    src/Scene.cpp
    src/TransformSystem.cpp
    src/RenderSystem.cpp
)
target_include_directories(me_scene PUBLIC include)
target_compile_features(me_scene PUBLIC cxx_std_17)
# 单向依赖:scene → core, assets。严禁依赖 rhi / renderer。
target_link_libraries(me_scene PUBLIC me_core me_assets)
