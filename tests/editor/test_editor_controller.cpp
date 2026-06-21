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
