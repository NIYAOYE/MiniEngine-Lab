#include "me/toolserver/ToolDispatcher.h"

#include <optional>

#include <nlohmann/json.hpp>

#include "me/toolapi/Permission.h"
#include "me/toolapi/ToolContext.h"
#include "me/toolapi/ToolRegistry.h"
#include "me/toolapi/ToolResult.h"

namespace me::toolserver {
namespace {

using me::toolapi::CallerRole;
using me::toolapi::ToolErrorCode;
using me::toolapi::ToolResult;

/// @brief 字符串 → CallerRole;非法返回 nullopt。
std::optional<CallerRole> ParseRole(const std::string& s) {
    if (s == "Agent") return CallerRole::Agent;
    if (s == "Automation") return CallerRole::Automation;
    if (s == "Editor") return CallerRole::Editor;
    return std::nullopt;
}

/// @brief 把失败 ToolResult 序列化为 JSON 字符串(统一出口)。
std::string ErrorJson(ToolErrorCode code, const std::string& msg) {
    return ToolResult::Error(code, msg).toJson().dump();
}

} // namespace

ToolDispatcher::ToolDispatcher(me::toolapi::ToolContext& ctx, me::toolapi::ToolRegistry& registry)
    : ctx_(ctx), registry_(registry) {}

std::string ToolDispatcher::HandleInvoke(const std::string& jsonBody) {
    std::lock_guard<std::mutex> lock(mutex_);

    const nlohmann::json body = nlohmann::json::parse(jsonBody, nullptr, /*allow_exceptions=*/false);
    if (body.is_discarded() || !body.is_object()) {
        return ErrorJson(ToolErrorCode::InvalidParams, "request body is not a valid JSON object");
    }
    if (!body.contains("name") || !body["name"].is_string()) {
        return ErrorJson(ToolErrorCode::InvalidParams, "missing or non-string 'name'");
    }
    const std::string name = body["name"].get<std::string>();

    nlohmann::json params = nlohmann::json::object();
    if (body.contains("params")) {
        if (!body["params"].is_object()) {
            return ErrorJson(ToolErrorCode::InvalidParams, "'params' must be an object");
        }
        params = body["params"];
    }

    CallerRole role = CallerRole::Editor; // 缺省本地编辑器全权
    if (body.contains("role")) {
        if (!body["role"].is_string()) {
            return ErrorJson(ToolErrorCode::InvalidParams, "'role' must be a string");
        }
        const auto parsed = ParseRole(body["role"].get<std::string>());
        if (!parsed) {
            return ErrorJson(ToolErrorCode::InvalidParams, "unknown role");
        }
        role = *parsed;
    }

    bool dryRun = false;
    if (body.contains("dryRun")) {
        if (!body["dryRun"].is_boolean()) {
            return ErrorJson(ToolErrorCode::InvalidParams, "'dryRun' must be a boolean");
        }
        dryRun = body["dryRun"].get<bool>();
    }

    const ToolResult result = registry_.Invoke(name, params, role, ctx_, dryRun);
    return result.toJson().dump();
}

std::string ToolDispatcher::HandleListTools() {
    std::lock_guard<std::mutex> lock(mutex_);
    return nlohmann::json::array().dump(); // Task 4 补实现
}

} // namespace me::toolserver
