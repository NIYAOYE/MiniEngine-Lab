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

// TimeStep → JSON(仅本 TU 内 time.advance 复用,故置匿名命名空间不外泄符号)。
nlohmann::json TimeStepToJson(const me::domain::TimeStep& s) {
    return nlohmann::json{
        {"minutesAdvanced", s.minutesAdvanced}, {"daysCrossed", s.daysCrossed},
        {"seasonsCrossed", s.seasonsCrossed},   {"yearsCrossed", s.yearsCrossed},
    };
}

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

// time.advance:推进 N 分钟。变更运行时态——刻意不经 CommandStack(时间不可 Undo,见 ADR 0006)。
// dry-run 在值拷贝副本上推进 → 零副作用。
class TimeAdvanceTool final : public ITool {
public:
    std::string name() const override { return "time.advance"; }
    ToolCategory category() const override { return ToolCategory::Mutation; }
    Permission permission() const override { return Permission::Automation; }
    nlohmann::json paramsSchema() const override {
        return {{"type", "object"},
                {"required", {"minutes"}},
                {"properties", {{"minutes", {{"type", "integer"}, {"minimum", 1}}}}}};
    }
    ToolResult dryRun(ToolContext& ctx, const nlohmann::json& p) const override {
        if (ctx.time == nullptr)
            return ToolResult::Error(ToolErrorCode::PreconditionFailed,
                                     "no time system wired into ToolContext");
        me::domain::TimeSystem preview = *ctx.time; // 值拷贝:在副本上推进即零副作用
        const me::domain::TimeStep step = preview.Advance(p["minutes"].get<int>());
        return ToolResult::Success(
            {{"step", TimeStepToJson(step)}, {"time", CalendarToJson(preview.Now())}});
    }
    ToolResult run(ToolContext& ctx, const nlohmann::json& p) const override {
        if (ctx.time == nullptr)
            return ToolResult::Error(ToolErrorCode::PreconditionFailed,
                                     "no time system wired into ToolContext");
        const me::domain::TimeStep step = ctx.time->Advance(p["minutes"].get<int>());
        return ToolResult::Success(
            {{"step", TimeStepToJson(step)}, {"time", CalendarToJson(ctx.time->Now())}});
    }
};

} // namespace

std::unique_ptr<ITool> MakeTimeGetTool() { return std::make_unique<TimeGetTool>(); }
std::unique_ptr<ITool> MakeTimeAdvanceTool() { return std::make_unique<TimeAdvanceTool>(); }

} // namespace me::toolapi
