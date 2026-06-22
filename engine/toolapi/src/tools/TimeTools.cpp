#include <memory>

#include "me/domain/TimeSystem.h"
#include "me/toolapi/ToolContext.h"
#include "me/toolapi/tools/BuiltinTools.h"

namespace me::toolapi {

nlohmann::json CalendarToJson(const me::domain::CalendarTime& c) {
    return nlohmann::json{
        {"year", c.year},           {"season", c.season},
        {"seasonName", c.seasonName},{"dayOfSeason", c.dayOfSeason},
        {"minuteOfDay", c.minuteOfDay},{"hour", c.hour},{"minute", c.minute},
    };
}

namespace {

// time.get:返回当前日历视图(只读)。
class TimeGetTool final : public ITool {
public:
    std::string name() const override { return "time.get"; }
    ToolCategory category() const override { return ToolCategory::Query; }
    Permission permission() const override { return Permission::AgentAllowed; }
    nlohmann::json paramsSchema() const override {
        return {{"type", "object"}, {"properties", nlohmann::json::object()}};
    }
    ToolResult dryRun(ToolContext& ctx, const nlohmann::json& p) const override {
        return run(ctx, p); // 只读:预览即结果
    }
    ToolResult run(ToolContext& ctx, const nlohmann::json&) const override {
        if (ctx.time == nullptr)
            return ToolResult::Error(ToolErrorCode::PreconditionFailed,
                                     "no time system wired into ToolContext");
        return ToolResult::Success(CalendarToJson(ctx.time->Now()));
    }
};

} // namespace

std::unique_ptr<ITool> MakeTimeGetTool() { return std::make_unique<TimeGetTool>(); }

} // namespace me::toolapi
