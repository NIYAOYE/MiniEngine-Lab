if(WIN32)
    add_library(me_renderer STATIC
        src/SpriteBatch.cpp
        src/TileMapRenderer.cpp
    )
    target_include_directories(me_renderer PUBLIC include)
    target_compile_features(me_renderer PUBLIC cxx_std_17)
    # 单向依赖:renderer → core, rhi(含 me_rhi_cpu), assets。
    target_link_libraries(me_renderer PUBLIC me_core me_rhi me_rhi_cpu me_assets)
endif()
