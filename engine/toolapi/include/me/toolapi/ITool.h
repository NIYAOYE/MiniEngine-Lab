#pragma once

#include <string>

#include <nlohmann/json.hpp>

#include "me/toolapi/Permission.h"
#include "me/toolapi/ToolResult.h"

namespace me::toolapi {

struct ToolContext; // 前置声明:ITool 仅按引用使用,避免拉入 Scene/Command 头

/// @brief Tool 类别:只读查询 vs 变更(变更必经 Command)。
enum class ToolCategory { Query, Mutation };

/**
 * @brief 统一 Tool 接口。无状态;所有引擎访问经 ToolContext。
 *
 * 变更型 Tool 的 run 必须构造 ICommand 经 CommandStack 执行(获得 Undo);
 * dryRun 返回预览且零副作用。
 */
class ITool {
public:
    virtual ~ITool() = default;

    /// @brief 唯一名,如 "scene.create_entity"。
    virtual std::string name() const = 0;
    /// @brief 类别(query/mutation)。
    virtual ToolCategory category() const = 0;
    /// @brief 所需最低权限(白名单)。
    virtual Permission permission() const = 0;
    /// @brief 参数的 JSON Schema 子集(供 Registry 自动校验)。
    virtual nlohmann::json paramsSchema() const = 0;

    /// @brief 预演:校验后产出"将会发生什么",零副作用。
    virtual ToolResult dryRun(ToolContext& ctx, const nlohmann::json& params) const = 0;
    /// @brief 执行:变更型构造 Command 经 CommandStack 落地。
    virtual ToolResult run(ToolContext& ctx, const nlohmann::json& params) const = 0;
};

} // namespace me::toolapi
