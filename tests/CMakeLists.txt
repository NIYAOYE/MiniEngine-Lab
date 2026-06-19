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
)

target_link_libraries(me_tests PRIVATE doctest::doctest me_core me_platform me_rhi_cpu me_assets)

add_test(NAME me_tests COMMAND me_tests)
