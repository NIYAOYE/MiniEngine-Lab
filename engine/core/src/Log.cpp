#include "me/core/Log.h"

#include <cstdio>

namespace me {

namespace {
const char* LevelTag(LogLevel level) {
    switch (level) {
        case LogLevel::Trace:   return "TRACE";
        case LogLevel::Info:    return "INFO";
        case LogLevel::Warning: return "WARN";
        case LogLevel::Error:   return "ERROR";
    }
    return "INFO"; // 不可达:全部枚举已覆盖
}
} // namespace

std::string FormatLogLine(LogLevel level, const std::string& msg) {
    std::string out;
    out.reserve(msg.size() + 8);
    out += '[';
    out += LevelTag(level);
    out += "] ";
    out += msg;
    return out;
}

void LogWrite(LogLevel level, const std::string& msg) {
    const std::string line = FormatLogLine(level, msg);
    std::fprintf(stderr, "%s\n", line.c_str());
}

} // namespace me
