#pragma once

#include <string>

namespace me {

/** @brief 日志级别。 */
enum class LogLevel { Trace, Info, Warning, Error };

/** @brief 把级别+消息格式化为单行字符串(纯函数,便于单测)。 */
std::string FormatLogLine(LogLevel level, const std::string& msg);

/** @brief 将一行日志写到标准错误输出。 */
void LogWrite(LogLevel level, const std::string& msg);

} // namespace me

// 便捷宏:参数为 std::string(M0 不做 printf 风格变参,保持简单/安全)。
#define ME_LOG_TRACE(msg) ::me::LogWrite(::me::LogLevel::Trace,   (msg))
#define ME_LOG_INFO(msg)  ::me::LogWrite(::me::LogLevel::Info,    (msg))
#define ME_LOG_WARN(msg)  ::me::LogWrite(::me::LogLevel::Warning, (msg))
#define ME_LOG_ERROR(msg) ::me::LogWrite(::me::LogLevel::Error,   (msg))
