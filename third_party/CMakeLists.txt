# M0 唯一第三方依赖:doctest(单头单测框架)
include(FetchContent)

FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG        v2.4.11
)
FetchContent_MakeAvailable(doctest)
# 提供 target: doctest::doctest
