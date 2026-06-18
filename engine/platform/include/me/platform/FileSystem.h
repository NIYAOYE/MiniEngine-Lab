#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace me::platform {

/** @brief 路径是否存在且为常规文件。 */
bool Exists(const std::string& path);

/** @brief 读取整个文本文件;失败返回 std::nullopt(不抛异常)。 */
std::optional<std::string> ReadTextFile(const std::string& path);

/** @brief 读取整个二进制文件;失败返回 std::nullopt(不抛异常)。 */
std::optional<std::vector<std::uint8_t>> ReadBinaryFile(const std::string& path);

/** @brief 写文本文件(覆盖);成功返回 true。 */
bool WriteTextFile(const std::string& path, const std::string& content);

/** @brief 写二进制文件(覆盖);成功返回 true。 */
bool WriteBinaryFile(const std::string& path, const std::vector<std::uint8_t>& bytes);

} // namespace me::platform
