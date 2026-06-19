add_library(me_platform STATIC
    src/Platform.cpp
    src/Time.cpp
    src/FileSystem.cpp
)

if(WIN32)
    target_sources(me_platform PRIVATE
        src/Window_Win32.cpp
    )
endif()

target_include_directories(me_platform PUBLIC include)
target_compile_features(me_platform PUBLIC cxx_std_17)

# 单向依赖:platform → core(core 不得反向依赖 platform)
target_link_libraries(me_platform PUBLIC me_core)
