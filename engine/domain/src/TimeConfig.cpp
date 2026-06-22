#include "me/domain/TimeConfig.h"

namespace me::domain {
namespace {

// 读一个必需的整数字段;缺失或非整数返回 false。
bool ReadInt(const nlohmann::json& j, const char* key, int& out) {
    if (!j.contains(key) || !j[key].is_number_integer()) return false;
    out = j[key].get<int>();
    return true;
}

} // namespace

std::optional<TimeConfig> LoadTimeConfig(const nlohmann::json& j) {
    if (!j.is_object()) return std::nullopt;

    TimeConfig c;
    if (!ReadInt(j, "minutesPerDay", c.minutesPerDay)) return std::nullopt;
    if (!ReadInt(j, "daysPerSeason", c.daysPerSeason)) return std::nullopt;
    if (!ReadInt(j, "seasonsPerYear", c.seasonsPerYear)) return std::nullopt;
    if (!ReadInt(j, "gameMinutesPerStep", c.gameMinutesPerStep)) return std::nullopt;
    if (!ReadInt(j, "startYear", c.startYear)) return std::nullopt;
    if (!ReadInt(j, "startSeason", c.startSeason)) return std::nullopt;
    if (!ReadInt(j, "startDay", c.startDay)) return std::nullopt;
    if (!ReadInt(j, "startMinute", c.startMinute)) return std::nullopt;

    if (!j.contains("realSecondsPerStep") || !j["realSecondsPerStep"].is_number())
        return std::nullopt;
    c.realSecondsPerStep = j["realSecondsPerStep"].get<double>();

    if (!j.contains("seasonNames") || !j["seasonNames"].is_array()) return std::nullopt;
    for (const auto& n : j["seasonNames"]) {
        if (!n.is_string()) return std::nullopt;
        c.seasonNames.push_back(n.get<std::string>());
    }

    // —— 语义校验(不变量,违反即非法配置)——
    if (c.minutesPerDay <= 0 || c.daysPerSeason <= 0 || c.seasonsPerYear <= 0)
        return std::nullopt;
    if (c.gameMinutesPerStep <= 0 || c.realSecondsPerStep <= 0.0) return std::nullopt;
    if (static_cast<int>(c.seasonNames.size()) != c.seasonsPerYear) return std::nullopt;
    if (c.startSeason < 0 || c.startSeason >= c.seasonsPerYear) return std::nullopt;
    if (c.startDay < 1 || c.startDay > c.daysPerSeason) return std::nullopt;
    if (c.startMinute < 0 || c.startMinute >= c.minutesPerDay) return std::nullopt;

    return c;
}

} // namespace me::domain
