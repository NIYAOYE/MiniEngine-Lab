#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

#include "me/toolapi/ITool.h"
#include "me/toolapi/Permission.h"
#include "me/toolapi/ToolContext.h"
#include "me/toolapi/ToolResult.h"

namespace me::toolapi {

/**
 * @brief Tool 注册表 + 统一执行流水线。
 *
 * Invoke 每次都过:1) 查找 → 2) 权限白名单 → 3) Schema 校验 →
 * 4) dryRun? 预览 → 5) run 落地 → 6) 记录 ToolInvocation(成功失败都记)。
 */
class ToolRegistry {
public:
    /// @brief 注册 Tool;重名返回 false 且不接管所有权丢弃。
    bool Register(std::unique_ptr<ITool> tool);
    /// @brief 按名查找;不存在返回 nullptr。
    const ITool* Find(const std::string& name) const;
    /// @brief 列出全部 Tool 名(字典序)。
    std::vector<std::string> ListNames() const;
    /// @brief 已注册 Tool 数。
    std::size_t Size() const { return m_tools.size(); }

    /**
     * @brief 统一调用入口。
     * @param name    Tool 名。
     * @param params  JSON 参数。
     * @param role    调用者角色(白名单裁决)。
     * @param ctx     受控上下文(同时承载审计日志)。
     * @param dryRun  true 则只预览不落地。
     * @return 统一 ToolResult,invocationId 已回填。
     */
    ToolResult Invoke(const std::string& name, const nlohmann::json& params,
                      CallerRole role, ToolContext& ctx, bool dryRun = false) const;

private:
    std::unordered_map<std::string, std::unique_ptr<ITool>> m_tools;
};

} // namespace me::toolapi
