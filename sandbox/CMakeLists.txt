add_executable(sandbox main.cpp)
target_compile_features(sandbox PRIVATE cxx_std_17)
target_link_libraries(sandbox PRIVATE
    me_core me_platform me_rhi me_rhi_cpu me_renderer me_assets me_scene)
# ME_ASSET_DIR 经 me_rhi 的 PUBLIC 编译定义传递,sandbox 直接可用。
