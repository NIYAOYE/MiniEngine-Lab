add_library(me_core STATIC
    src/Version.cpp
    src/Vector2.cpp
    src/Vector3.cpp
)

target_include_directories(me_core PUBLIC include)
target_compile_features(me_core PUBLIC cxx_std_17)
