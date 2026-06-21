add_executable(me_tests
    test_main.cpp
    core/test_smoke.cpp
    core/test_vector2.cpp
    core/test_vector34.cpp
    core/test_matrix.cpp
    core/test_transform2d.cpp
    core/test_rect_aabb.cpp
    core/test_handle.cpp
    core/test_log.cpp
    platform/test_time.cpp
    platform/test_filesystem.cpp
    platform/test_input.cpp
    rhi/test_rhi_logic.cpp
    assets/test_image.cpp
    assets/test_tile_layout.cpp
    assets/test_tile_geometry.cpp
    assets/test_tiled_loader.cpp
    render/test_camera.cpp
    scene/test_entity.cpp
    scene/test_entity_id.cpp
    scene/test_component_snapshot.cpp
    scene/test_transform_hierarchy.cpp
    scene/test_component_storage.cpp
    scene/test_render_system.cpp
    scene/test_camera_system.cpp
    command/test_command_stack.cpp
    command/test_create_entity_cmd.cpp
    command/test_set_transform_cmd.cpp
    command/test_destroy_entity_cmd.cpp
    toolapi/test_tool_result.cpp
    toolapi/test_permission.cpp
    toolapi/test_schema_validator.cpp
    toolapi/test_invocation_log.cpp
    toolapi/test_registry.cpp
)

target_link_libraries(me_tests PRIVATE doctest::doctest me_core me_platform me_rhi_cpu me_assets me_scene me_command me_toolapi)

target_include_directories(me_tests PRIVATE ${CMAKE_SOURCE_DIR}/engine/renderer/include)

target_compile_definitions(me_tests PRIVATE
    ME_TEST_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets/fixtures")

add_test(NAME me_tests COMMAND me_tests)
