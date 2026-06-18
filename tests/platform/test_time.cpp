#include <doctest/doctest.h>

#include "me/platform/Time.h"

#include <thread>
#include <chrono>

using me::platform::Stopwatch;
using me::platform::FrameTimer;

TEST_CASE("stopwatch elapsed is non-negative and monotonic") {
    Stopwatch sw;
    const double t1 = sw.ElapsedSeconds();
    const double t2 = sw.ElapsedSeconds();
    CHECK(t1 >= 0.0);
    CHECK(t2 >= t1);
    CHECK(sw.ElapsedMilliseconds() >= 0.0);
}

TEST_CASE("stopwatch measures a real delay") {
    Stopwatch sw;
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    CHECK(sw.ElapsedSeconds() >= 0.010); // 宽松下界,避开调度抖动
}

TEST_CASE("frame timer first tick is zero, total accumulates") {
    FrameTimer ft;
    CHECK(ft.Tick() == doctest::Approx(0.0)); // 首帧无历史
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    const double dt = ft.Tick();
    CHECK(dt >= 0.0);
    CHECK(ft.TotalSeconds() >= dt);
}
