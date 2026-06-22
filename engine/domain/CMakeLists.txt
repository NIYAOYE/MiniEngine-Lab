add_library(me_domain STATIC
    src/TimeConfig.cpp
    src/TimeSystem.cpp
    src/CropConfig.cpp
)
target_include_directories(me_domain PUBLIC include)
target_compile_features(me_domain PUBLIC cxx_std_17)
# 单向依赖:domain → core。nlohmann/json 出现在公开头(TimeConfig.h)→ PUBLIC。
# 严禁依赖 scene / command / toolapi / rhi / renderer。
target_link_libraries(me_domain PUBLIC me_core nlohmann_json::nlohmann_json)
