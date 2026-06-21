add_library(me_editor STATIC
    src/EditorController.cpp
)
target_include_directories(me_editor PUBLIC include)
target_compile_features(me_editor PUBLIC cxx_std_17)
# 单向依赖:editor → toolapi → command → scene → core。严禁依赖 rhi / renderer / platform / imgui。
# nlohmann/json 出现在解析路径(非公开头),但 DTO 头不含 json → 仅 PRIVATE 即可;
# 然 ToolContext.h/ToolRegistry.h(PUBLIC 头引用)已 PUBLIC 传递 json,故用 PUBLIC 链接 toolapi。
target_link_libraries(me_editor PUBLIC me_toolapi me_command me_scene me_core nlohmann_json::nlohmann_json)
