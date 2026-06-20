add_library(me_assets STATIC
    src/ImageLoader.cpp
    src/stb_image_impl.cpp
    src/TiledMapLoader.cpp
)
target_include_directories(me_assets PUBLIC include)
target_compile_features(me_assets PUBLIC cxx_std_17)
# 单向依赖:assets → core, platform;stb_image / nlohmann_json 仅私有实现细节。
target_link_libraries(me_assets PUBLIC me_core me_platform
                                 PRIVATE stb_image nlohmann_json::nlohmann_json)
