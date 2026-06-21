#include "me/toolapi/ToolRegistry.h"

#include <algorithm>

#include "me/toolapi/SchemaValidator.h"

namespace me::toolapi {
namespace {

// 把一次调用的结果落入审计日志,并把分配的 id 回填到结果上。
void Record(ToolContext& ctx, const std::string& name, const nlohmann::json& params,
            bool dryRun, ToolResult& result) {
    ToolInvocation inv;
    inv.tool = name;
    inv.params = params;
    inv.dryRun = dryRun;
    inv.ok = result.ok;
    inv.code = result.code;
    inv.message = result.message;
    result.invocationId = ctx.log.Append(std::move(inv));
}

} // namespace

bool ToolRegistry::Register(std::unique_ptr<ITool> tool) {
    if (!tool) return false;
    const std::string key = tool->name();
    if (m_tools.find(key) != m_tools.end()) return false; // 重名拒绝
    m_tools.emplace(key, std::move(tool));
    return true;
}

const ITool* ToolRegistry::Find(const std::string& name) const {
    auto it = m_tools.find(name);
    return it == m_tools.end() ? nullptr : it->second.get();
}

std::vector<std::string> ToolRegistry::ListNames() const {
    std::vector<std::string> names;
    names.reserve(m_tools.size());
    for (const auto& kv : m_tools) names.push_back(kv.first);
    std::sort(names.begin(), names.end());
    return names;
}

ToolResult ToolRegistry::Invoke(const std::string& name, const nlohmann::json& params,
                                CallerRole role, ToolContext& ctx, bool dryRun) const {
    // 1) 查找
    const ITool* tool = Find(name);
    if (!tool) {
        ToolResult r = ToolResult::Error(ToolErrorCode::UnknownTool, "unknown tool: " + name);
        Record(ctx, name, params, dryRun, r);
        return r;
    }
    // 2) 权限白名单
    if (!IsAllowed(role, tool->permission())) {
        ToolResult r = ToolResult::Error(ToolErrorCode::PermissionDenied,
                                         "caller not permitted for tool: " + name);
        Record(ctx, name, params, dryRun, r);
        return r;
    }
    // 3) Schema 校验
    const ValidationResult v = ValidateAgainstSchema(tool->paramsSchema(), params);
    if (!v.ok) {
        ToolResult r = ToolResult::Error(ToolErrorCode::InvalidParams, "invalid params",
                                         {{"errors", v.errors}});
        Record(ctx, name, params, dryRun, r);
        return r;
    }
    // 4/5) 预览或落地
    ToolResult r = dryRun ? tool->dryRun(ctx, params) : tool->run(ctx, params);
    // 6) 记录
    Record(ctx, name, params, dryRun, r);
    return r;
}

} // namespace me::toolapi
