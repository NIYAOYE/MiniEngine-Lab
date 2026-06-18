#pragma once

#include <chrono>

namespace me::platform {

/** @brief 单调高精度秒表(基于 steady_clock)。 */
class Stopwatch {
public:
    /** @brief 构造即开始计时。 */
    Stopwatch() : m_start(Clock::now()) {}

    /** @brief 重置起点为当前时刻。 */
    void Restart() { m_start = Clock::now(); }

    /** @brief 自起点经过的秒数。 */
    double ElapsedSeconds() const;
    /** @brief 自起点经过的毫秒数。 */
    double ElapsedMilliseconds() const;

private:
    using Clock = std::chrono::steady_clock;
    Clock::time_point m_start;
};

/** @brief 逐帧计时器:每次 Tick 返回距上次 Tick 的秒数。 */
class FrameTimer {
public:
    FrameTimer();

    /** @brief 推进一帧,返回 deltaTime(秒);首次调用返回 0。 */
    double Tick();
    /** @brief 自创建以来累计的总秒数。 */
    double TotalSeconds() const { return m_total; }

private:
    using Clock = std::chrono::steady_clock;
    Clock::time_point m_last;
    double m_total = 0.0;
    bool   m_firstTick = true;
};

} // namespace me::platform
