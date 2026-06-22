#include "me/domain/TimeSystem.h"

#include <utility>

#include "me/core/Assert.h"

namespace me::domain {
namespace {

// 计算 [0, ...) 两个绝对分钟数落在不同 unit 桶时的跨界次数(floor 除法之差)。
int CrossCount(long long before, long long after, long long unit) {
    return static_cast<int>(after / unit - before / unit);
}

constexpr int kMinutesPerHour = 60; // 现实分钟/小时(非游戏可调值)

} // namespace

TimeSystem::TimeSystem(TimeConfig config) : config_(std::move(config)) {
    const long long mpd = config_.minutesPerDay;
    const long long mps = mpd * config_.daysPerSeason;
    // 纪元 = (startYear, season 0, day 1, minute 0);start* 给出纪元内偏移。
    totalMinutes_ = static_cast<long long>(config_.startSeason) * mps
                  + static_cast<long long>(config_.startDay - 1) * mpd
                  + config_.startMinute;
}

TimeStep TimeSystem::Advance(int minutes) {
    ME_ASSERT(minutes >= 1); // 不变量:推进必为正整数分钟
    const long long mpd = config_.minutesPerDay;
    const long long mps = mpd * config_.daysPerSeason;
    const long long mpy = mps * config_.seasonsPerYear;

    const long long before = totalMinutes_;
    const long long after = before + minutes;

    TimeStep step;
    step.minutesAdvanced = minutes;
    step.daysCrossed = CrossCount(before, after, mpd);
    step.seasonsCrossed = CrossCount(before, after, mps);
    step.yearsCrossed = CrossCount(before, after, mpy);

    totalMinutes_ = after;
    return step;
}

TimeStep TimeSystem::Update(double) {
    return {}; // Task 3 实现
}

CalendarTime TimeSystem::CalendarAt(long long total) const {
    const long long mpd = config_.minutesPerDay;
    const long long mps = mpd * config_.daysPerSeason;
    const long long mpy = mps * config_.seasonsPerYear;

    const long long years = total / mpy;
    long long rem = total % mpy;
    const long long season = rem / mps;
    rem = rem % mps;
    const long long dayIdx = rem / mpd;
    const long long minOfDay = rem % mpd;

    CalendarTime t;
    t.year = config_.startYear + static_cast<int>(years);
    t.season = static_cast<int>(season);
    ME_ASSERT(static_cast<std::size_t>(season) < config_.seasonNames.size());
    t.seasonName = config_.seasonNames[static_cast<std::size_t>(season)];
    t.dayOfSeason = static_cast<int>(dayIdx) + 1;
    t.minuteOfDay = static_cast<int>(minOfDay);
    t.hour = t.minuteOfDay / kMinutesPerHour;
    t.minute = t.minuteOfDay % kMinutesPerHour;
    return t;
}

CalendarTime TimeSystem::Now() const { return CalendarAt(totalMinutes_); }

} // namespace me::domain
