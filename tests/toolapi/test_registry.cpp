#include <doctest/doctest.h>

#include <memory>

#include "me/command/CommandStack.h"
#include "me/scene/Scene.h"
#include "me/toolapi/ITool.h"
#include "me/toolapi/ToolContext.h"
#include "me/toolapi/ToolRegistry.h"

using namespace me::toolapi;

namespace {
// 测试桩:可配置权限,run 写一个计数器以验证副作用是否发生。
class StubTool final : public ITool {
public:
    StubTool(std::string n, Permission p, int* runCounter)
        : m_name(std::move(n)), m_perm(p), m_runs(runCounter) {}
    std::string name() const override { return m_name; }
    ToolCategory category() const override { return ToolCategory::Mutation; }
    Permission permission() const override { return m_perm; }
    nlohmann::json paramsSchema() const override {
        return {{"type", "object"},
                {"required", {"x"}},
                {"properties", {{"x", {{"type", "integer"}}}}}};
    }
    ToolResult dryRun(ToolContext&, const nlohmann::json&) const override {
        return ToolResult::Success({{"preview", "would run"}});
    }
    ToolResult run(ToolContext&, const nlohmann::json& p) const override {
        ++(*m_runs);
        return ToolResult::Success({{"x", p["x"]}});
    }

private:
    std::string m_name;
    Permission m_perm;
    int* m_runs;
};
} // namespace

TEST_CASE("Registry:Register/Find/ListNames/重名拒绝") {
    ToolRegistry reg;
    int runs = 0;
    CHECK(reg.Register(std::make_unique<StubTool>("b.tool", Permission::AgentAllowed, &runs)));
    CHECK(reg.Register(std::make_unique<StubTool>("a.tool", Permission::AgentAllowed, &runs)));
    CHECK_FALSE(reg.Register(std::make_unique<StubTool>("a.tool", Permission::AgentAllowed, &runs)));
    CHECK(reg.Size() == 2);
    CHECK(reg.Find("a.tool") != nullptr);
    CHECK(reg.Find("missing") == nullptr);
    auto names = reg.ListNames();
    CHECK(names.size() == 2);
    CHECK(names[0] == "a.tool"); // 排序
    CHECK(names[1] == "b.tool");
}

TEST_CASE("Registry:Invoke 流水线错误码与副作用") {
    ToolRegistry reg;
    int runs = 0;
    reg.Register(std::make_unique<StubTool>("safe.tool", Permission::AgentAllowed, &runs));
    reg.Register(std::make_unique<StubTool>("danger.tool", Permission::EditorOnly, &runs));

    me::scene::Scene scene;
    me::command::CommandStack stack;
    ToolInvocationLog log;
    ToolContext ctx{scene, stack, log};

    SUBCASE("UnknownTool") {
        auto r = reg.Invoke("nope", {{"x", 1}}, CallerRole::Editor, ctx);
        CHECK(r.code == ToolErrorCode::UnknownTool);
        CHECK(runs == 0);
        CHECK(r.invocationId == 1);
        CHECK(log.Size() == 1);
    }
    SUBCASE("PermissionDenied:Agent 调 EditorOnly") {
        auto r = reg.Invoke("danger.tool", {{"x", 1}}, CallerRole::Agent, ctx);
        CHECK(r.code == ToolErrorCode::PermissionDenied);
        CHECK(runs == 0);
        // 失败路径也必须进审计日志并回填 invocationId
        CHECK(r.invocationId == 1);
        CHECK(log.Size() == 1);
        CHECK(log.Entries()[0].code == ToolErrorCode::PermissionDenied);
    }
    SUBCASE("InvalidParams:缺 required") {
        auto r = reg.Invoke("safe.tool", {{"y", 1}}, CallerRole::Agent, ctx);
        CHECK(r.code == ToolErrorCode::InvalidParams);
        CHECK(r.data["errors"].size() >= 1);
        CHECK(runs == 0);
        // 校验失败同样记录,审计完整
        CHECK(r.invocationId == 1);
        CHECK(log.Size() == 1);
        CHECK(log.Entries()[0].code == ToolErrorCode::InvalidParams);
    }
    SUBCASE("dryRun 零副作用但仍记录") {
        auto r = reg.Invoke("safe.tool", {{"x", 1}}, CallerRole::Agent, ctx, /*dryRun=*/true);
        CHECK(r.ok);
        CHECK(r.data["preview"] == "would run");
        CHECK(runs == 0);
        CHECK(log.Size() == 1);
        CHECK(log.Entries()[0].dryRun == true);
    }
    SUBCASE("run 成功并记录 invocationId") {
        auto r = reg.Invoke("safe.tool", {{"x", 42}}, CallerRole::Agent, ctx);
        CHECK(r.ok);
        CHECK(r.data["x"] == 42);
        CHECK(runs == 1);
        CHECK(r.invocationId == 1);
    }
    SUBCASE("每次调用都进日志(含失败)") {
        reg.Invoke("nope", {}, CallerRole::Editor, ctx);            // UnknownTool
        reg.Invoke("safe.tool", {{"x", 1}}, CallerRole::Agent, ctx); // ok
        CHECK(log.Size() == 2);
        CHECK(log.Entries()[0].code == ToolErrorCode::UnknownTool);
        CHECK(log.Entries()[1].ok);
    }
}
