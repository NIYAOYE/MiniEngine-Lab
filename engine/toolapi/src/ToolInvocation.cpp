#include "me/toolapi/ToolInvocation.h"

namespace me::toolapi {

nlohmann::json ToolInvocation::toJson() const {
    return nlohmann::json{
        {"id", id},
        {"tool", tool},
        {"params", params},
        {"dryRun", dryRun},
        {"ok", ok},
        {"code", ToString(code)},
        {"message", message},
    };
}

std::uint64_t ToolInvocationLog::Append(ToolInvocation inv) {
    inv.id = ++m_nextId;
    m_entries.push_back(std::move(inv));
    return m_nextId;
}

} // namespace me::toolapi
