#include "me/platform/Time.h"

namespace me::platform {

double Stopwatch::ElapsedSeconds() const {
    const std::chrono::duration<double> d = Clock::now() - m_start;
    return d.count();
}

double Stopwatch::ElapsedMilliseconds() const {
    const std::chrono::duration<double, std::milli> d = Clock::now() - m_start;
    return d.count();
}

FrameTimer::FrameTimer() : m_last(Clock::now()) {}

double FrameTimer::Tick() {
    const Clock::time_point now = Clock::now();
    if (m_firstTick) {
        m_firstTick = false;
        m_last = now;
        return 0.0; // 首帧无历史,delta 视为 0
    }
    const std::chrono::duration<double> d = now - m_last;
    m_last = now;
    const double dt = d.count();
    m_total += dt;
    return dt;
}

} // namespace me::platform
