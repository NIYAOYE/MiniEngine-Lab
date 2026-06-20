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
    scene/test_transform_hierarchy.cpp
)

target_link_libraries(me_tests PRIVATE doctest::doctest me_core me_platform me_rhi_cpu me_assets me_scene)

target_include_directories(me_tests PRIVATE ${CMAKE_SOURCE_DIR}/engine/renderer/include)

target_compile_definitions(me_tests PRIVATE
    ME_TEST_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets/fixtures")

add_test(NAME me_tests COMMAND me_tests)
