#include <doctest/doctest.h>

#include "me/command/CommandStack.h"
#include "me/editor/EditorController.h"
#include "me/scene/Scene.h"
#include "me/toolapi/ToolContext.h"
#include "me/toolapi/ToolRegistry.h"
#include "me/toolapi/tools/BuiltinTools.h"

using namespace me::editor;

namespace {
// 测试夹具:registry(含 6 builtin) + scene + stack + log + ctx + controller。
struct Fixture {
    me::toolapi::ToolRegistry registry;
    me::scene::Scene scene;
    me::command::CommandStack stack;
    me::toolapi::ToolInvocationLog log;
    me::toolapi::ToolContext ctx{scene, stack, log};
    EditorController ctrl{registry, ctx};
    Fixture() { me::toolapi::RegisterBuiltinTools(registry); }
    // 经 create_entity Tool 直接造一个实体,返回其持久 id。
    me::scene::EntityId makeEntity() {
        auto r = registry.Invoke("scene.create_entity", nlohmann::json::object(),
                                 me::toolapi::CallerRole::Editor, ctx);
        REQUIRE(r.ok);
        return r.data["id"].get<me::scene::EntityId>();
    }
};
} // namespace

TEST_CASE("EditorController:空场景 RefreshHierarchy 得空列表、无错误") {
    Fixture f;
    f.ctrl.RefreshHierarchy();
    CHECK(f.ctrl.Hierarchy().empty());
    CHECK_FALSE(f.ctrl.HasError());
}

TEST_CASE("EditorController:RefreshHierarchy 反映已存在实体的身份与变换") {
    Fixture f;
    const auto id = f.makeEntity();
    f.ctrl.RefreshHierarchy();
    REQUIRE(f.ctrl.Hierarchy().size() == 1);
    CHECK(f.ctrl.Hierarchy()[0].id == id);
    // 新建实体为单位变换:scale 默认 1。
    CHECK(f.ctrl.Hierarchy()[0].localTransform.scale.x == doctest::Approx(1.0f));
}

TEST_CASE("EditorController:Select+InspectSelected 得到变换与层级") {
    Fixture f;
    const auto id = f.makeEntity();
    // 经 Tool 设一个非平凡变换,确认 inspect 能读回。
    f.registry.Invoke("entity.set_transform",
                      {{"id", id}, {"position", {{"x", 7.0}, {"y", 9.0}}}},
                      me::toolapi::CallerRole::Editor, f.ctx);
    f.ctrl.Select(id);
    f.ctrl.InspectSelected();
    REQUIRE(f.ctrl.HasInspected());
    CHECK(f.ctrl.Inspected().id == id);
    CHECK(f.ctrl.Inspected().localTransform.position.x == doctest::Approx(7.0f));
    CHECK(f.ctrl.Inspected().parentId == 0); // 无父
    CHECK(f.ctrl.Inspected().children.empty());
    CHECK_FALSE(f.ctrl.HasError());
}

TEST_CASE("EditorController:InspectSelected 不存在实体 → LastError,不崩") {
    Fixture f;
    f.ctrl.Select(9999); // 从未创建
    f.ctrl.InspectSelected();
    CHECK_FALSE(f.ctrl.HasInspected());
    CHECK(f.ctrl.HasError()); // 经 get_entity PreconditionFailed
}

TEST_CASE("EditorController:CreateEntity 实体+1、选中新实体、Hierarchy 刷新") {
    Fixture f;
    f.ctrl.CreateEntity();
    REQUIRE(f.ctrl.Hierarchy().size() == 1);
    CHECK(f.ctrl.HasSelection());
    CHECK(f.ctrl.Selected() == f.ctrl.Hierarchy()[0].id);
    CHECK(f.ctrl.HasInspected());
    CHECK_FALSE(f.ctrl.HasError());
}
