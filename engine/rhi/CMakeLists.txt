# 纯逻辑、跨平台、header-only:RHI 中可脱离 DX12 单测的部分。
add_library(me_rhi_cpu INTERFACE)
target_include_directories(me_rhi_cpu INTERFACE include)
target_compile_features(me_rhi_cpu INTERFACE cxx_std_17)
target_link_libraries(me_rhi_cpu INTERFACE me_core)

# DX12 实现层 me_rhi 仅在 Windows 构建(后续任务逐步填充源文件)。
if(WIN32)
    add_library(me_rhi STATIC
        src/D3DCommon.cpp  # Task 4 创建;后续任务继续追加源
    )
    target_include_directories(me_rhi PUBLIC include)
    target_compile_features(me_rhi PUBLIC cxx_std_17)
    target_link_libraries(me_rhi
        PUBLIC me_core me_platform me_rhi_cpu
        PRIVATE d3d12 dxgi d3dcompiler dxguid)
    # 让运行时能定位着色器/纹理:把仓库 assets 目录编进宏。
    target_compile_definitions(me_rhi PUBLIC ME_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets")
endif()
