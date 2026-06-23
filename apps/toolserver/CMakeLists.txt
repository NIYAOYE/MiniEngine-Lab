add_executable(toolserver_app main.cpp)
target_compile_features(toolserver_app PRIVATE cxx_std_17)
target_link_libraries(toolserver_app PRIVATE
    me_toolserver me_toolapi me_domain me_scene me_command me_platform me_core
    nlohmann_json::nlohmann_json)
# 无头:不链接 me_rhi/me_renderer/imgui。资产根经编译定义注入(me_rhi 的 ME_ASSET_DIR
# 是 Windows-only,本可执行跨平台,故自行定义)。
target_compile_definitions(toolserver_app PRIVATE ME_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets")
