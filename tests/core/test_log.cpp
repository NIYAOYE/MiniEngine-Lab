#include <doctest/doctest.h>

#include "me/core/Log.h"
#include "me/core/Assert.h"

using me::LogLevel;

TEST_CASE("format log line prepends level tag") {
    CHECK(me::FormatLogLine(LogLevel::Info, "hello") == "[INFO] hello");
    CHECK(me::FormatLogLine(LogLevel::Warning, "careful") == "[WARN] careful");
    CHECK(me::FormatLogLine(LogLevel::Error, "boom") == "[ERROR] boom");
    CHECK(me::FormatLogLine(LogLevel::Trace, "step") == "[TRACE] step");
}

TEST_CASE("log macros do not crash") {
    ME_LOG_INFO(std::string("info via macro"));
    ME_LOG_WARN(std::string("warn via macro"));
}

TEST_CASE("passing assert is a no-op") {
    ME_ASSERT(1 + 1 == 2);
    ME_ASSERT_MSG(true, "should hold");
    CHECK(true);
}
