# 纯逻辑、跨平台、header-only:RHI 中可脱离 DX12 单测的部分。
add_library(me_rhi_cpu INTERFACE)
target_include_directories(me_rhi_cpu INTERFACE include)
target_compile_features(me_rhi_cpu INTERFACE cxx_std_17)
target_link_libraries(me_rhi_cpu INTERFACE me_core)

# DX12 实现层 me_rhi 仅在 Windows 构建(后续任务逐步填充源文件)。
if(WIN32)
    add_library(me_rhi STATIC
        src/D3DCommon.cpp
        src/GpuDevice.cpp
        src/Fence.cpp
        src/DescriptorHeap.cpp
        src/CommandContext.cpp
        src/GpuBuffer.cpp
        src/GpuTexture.cpp
        src/Readback.cpp
        src/Shader.cpp
        src/SwapChain.cpp
    )
    target_include_directories(me_rhi PUBLIC include)
    target_compile_features(me_rhi PUBLIC cxx_std_17)
    target_link_libraries(me_rhi
        PUBLIC me_core me_platform me_rhi_cpu
        PRIVATE d3d12 dxgi d3dcompiler dxguid)
    # 让运行时能定位着色器/纹理:把仓库 assets 目录编进宏。
    target_compile_definitions(me_rhi PUBLIC ME_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets")

    # DX12/WARP GPU 测试(像素回读),仅在 ME_BUILD_GPU_TESTS=ON 时构建。
    if(ME_BUILD_GPU_TESTS)
        add_executable(me_gpu_tests
            ${CMAKE_SOURCE_DIR}/tests/gpu/gpu_test_main.cpp
            ${CMAKE_SOURCE_DIR}/tests/gpu/test_device_fence.cpp
            ${CMAKE_SOURCE_DIR}/tests/gpu/test_texture_roundtrip.cpp
            ${CMAKE_SOURCE_DIR}/tests/gpu/test_sprite_render.cpp
        )
        target_link_libraries(me_gpu_tests PRIVATE doctest::doctest me_rhi me_rhi_cpu me_renderer)
        add_test(NAME me_gpu_tests COMMAND me_gpu_tests)
    endif()
endif()
