#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "me/toolapi/ToolResult.h"

namespace me::toolapi {

/**
 * @brief 一次 Tool 调用的可序列化审计记录(成功与失败都记录)。
 *
 * dry-run + Command 回滚 + 完整 ToolInvocation 历史 = 能力可审计/可预览/可回滚。
 */
struct ToolInvocation {
    std::uint64_t id = 0;       ///< 由日志分配的单调 id
    std::string tool;           ///< Tool 名
    nlohmann::json params;      ///< 原始入参
    bool dryRun = false;        ///< 是否预演调用
    bool ok = true;             ///< 结果是否成功
    ToolErrorCode code = ToolErrorCode::Ok;
    std::string message;

    /// @brief 序列化为 JSON 记录。
    nlohmann::json toJson() const;
};

/// @brief 仅追加的调用日志;log.read 工具的数据源。
class ToolInvocationLog {
public:
    /// @brief 追加一条记录,分配单调 id(从 1 起)并回填,返回该 id。
    std::uint64_t Append(ToolInvocation inv);
    /// @brief 全部记录(追加顺序)。
    const std::vector<ToolInvocation>& Entries() const { return m_entries; }
    /// @brief 记录条数。
    std::size_t Size() const { return m_entries.size(); }

private:
    std::vector<ToolInvocation> m_entries;
    std::uint64_t m_nextId = 0; ///< ++ 后即 1,2,3...
};

} // namespace me::toolapi
