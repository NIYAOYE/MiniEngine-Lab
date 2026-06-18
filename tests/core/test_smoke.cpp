#include <doctest/doctest.h>

#include "me/core/Version.h"
#include "me/platform/Platform.h"

#include <cstring>

TEST_CASE("engine name is MiniEngine") {
    CHECK(std::strcmp(me::EngineName(), "MiniEngine") == 0);
}

TEST_CASE("platform name is non-empty") {
    CHECK(std::strlen(me::platform::PlatformName()) > 0);
}
