#pragma once

#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace me::domain {

/**
 * @brief 数据驱动的日历参数(全部从 JSON 加载,源码零硬编码数值)。
 *
 * 计数基准:season 0 基(seasonNames 下标);startDay 1 基;
 * startMinute 0 基,范围 [0, minutesPerDay)。
 */
struct TimeConfig {
    int minutesPerDay = 0;            ///< 一天的游戏分钟数
    int daysPerSeason = 0;            ///< 一个季节的天数
    int seasonsPerYear = 0;           ///< 一年的季节数
    std::vector<std::string> seasonNames; ///< 季节名(数量须 == seasonsPerYear)
    int gameMinutesPerStep = 0;       ///< 一个推进步进推多少游戏分钟
    double realSecondsPerStep = 0.0;  ///< 现实多少秒走一个步进(Update 用)
    int startYear = 0;                ///< 初始年(year 从此起算)
    int startSeason = 0;             ///< 初始季节(0 基)
    int startDay = 1;               ///< 初始天(1 基)
    int startMinute = 0;            ///< 初始分钟(0 基)
};

/**
 * @brief 从 JSON 解析并校验 TimeConfig。
 * @return 校验通过返回配置;任一字段缺失/类型错/语义越界返回 std::nullopt(不抛异常)。
 */
std::optional<TimeConfig> LoadTimeConfig(const nlohmann::json& j);

} // namespace me::domain
