#pragma once

#include <string>

#include "me/domain/TimeConfig.h"

namespace me::domain {

/// @brief 当前时间的派生视图(由 totalMinutes_ 按 config 算出)。
struct CalendarTime {
    int year = 0;        ///< startYear + 跨年数
    int season = 0;      ///< 0 基索引
    std::string seasonName;
    int dayOfSeason = 1; ///< 1 基
    int minuteOfDay = 0; ///< 0 基,[0, minutesPerDay)
    int hour = 0;        ///< minuteOfDay / 60
    int minute = 0;      ///< minuteOfDay % 60
};

/// @brief 一次推进跨越的边界计数(非布尔,保留多天跳跃信息)。
struct TimeStep {
    int minutesAdvanced = 0;
    int daysCrossed = 0;
    int seasonsCrossed = 0;
    int yearsCrossed = 0;
};

/**
 * @brief 四级日历时钟(分钟/天/季/年)。
 *
 * 内部以单调 totalMinutes_(自纪元起总分钟)为单一真相源,日历字段即时派生。
 * 值语义可拷贝:Tool 的 dry-run 通过拷贝实例在副本上推进实现零副作用。
 * 时间是运行时状态,不进 Command/Undo(见 ADR 0006)。
 */
class TimeSystem {
public:
    /// @brief 以 config 的 start* 字段为起点初始化时钟。
    explicit TimeSystem(TimeConfig config);

    /// @brief 显式推进整分钟(minutes 须 ≥ 1),返回跨界计数。
    TimeStep Advance(int minutes);

    /// @brief 按真实经过秒数累积推进;每满一步进吐 gameMinutesPerStep 分钟,返回聚合跨界计数。
    TimeStep Update(double realDeltaSeconds);

    /// @brief 当前日历视图。
    CalendarTime Now() const;

    /// @brief 只读访问配置。
    const TimeConfig& Config() const { return config_; }

private:
    /// @brief 把一个绝对总分钟数派生为日历视图。
    CalendarTime CalendarAt(long long totalMinutes) const;

    TimeConfig config_;
    long long totalMinutes_ = 0;        ///< 单一真相源
    double realSecondAccumulator_ = 0.0;///< Update 的小数余量
};

} // namespace me::domain
