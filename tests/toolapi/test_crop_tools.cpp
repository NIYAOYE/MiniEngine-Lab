#include <doctest/doctest.h>

#include "me/command/CommandStack.h"
#include "me/domain/FarmField.h"
#include "me/scene/Scene.h"
#include "me/toolapi/ToolContext.h"
#include "me/toolapi/ToolRegistry.h"
#include "me/toolapi/tools/BuiltinTools.h"

using namespace me::toolapi;

namespace {
me::domain::FarmField MakeFarm() {
    auto db = me::domain::LoadCropDatabase(nlohmann::json::array({
        {{"id", "parsnip"},
         {"name", "Parsnip"},
         {"stageNames", {"seed", "sprout", "growing", "mature"}},
         {"stageDays", {1, 1, 1, 1}},
         {"harvestItemId", "parsnip"},
         {"yield", 1}},
    }));
    REQUIRE(db.has_value());
    return me::domain::FarmField(*db);
}
} // namespace

TEST_CASE("CropTools:crop.plant 种植并 get_field 可见") {
    ToolRegistry reg;
    reg.Register(MakeCropPlantTool());
    reg.Register(MakeCropGetFieldTool());
    me::scene::Scene scene;
    me::command::CommandStack stack;
    ToolInvocationLog log;
    auto farm = MakeFarm();
    ToolContext ctx{scene, stack, log, nullptr, &farm};

    auto r = reg.Invoke("crop.plant", {{"tileX", 2}, {"tileY", 3}, {"cropId", "parsnip"}},
                        CallerRole::Automation, ctx);
    REQUIRE(r.ok);
    CHECK(r.data["cropId"] == "parsnip");
    CHECK(r.data["stage"] == 0);
    CHECK(r.data["mature"] == false);

    auto g = reg.Invoke("crop.get_field", nlohmann::json::object(), CallerRole::Agent, ctx);
    REQUIRE(g.ok);
    REQUIRE(g.data["crops"].size() == 1);
    CHECK(g.data["crops"][0]["x"] == 2);
    CHECK(g.data["crops"][0]["y"] == 3);
}

TEST_CASE("CropTools:crop.plant 重复瓦片 PreconditionFailed") {
    ToolRegistry reg;
    reg.Register(MakeCropPlantTool());
    me::scene::Scene scene;
    me::command::CommandStack stack;
    ToolInvocationLog log;
    auto farm = MakeFarm();
    ToolContext ctx{scene, stack, log, nullptr, &farm};

    REQUIRE(reg.Invoke("crop.plant", {{"tileX", 0}, {"tileY", 0}, {"cropId", "parsnip"}},
                       CallerRole::Automation, ctx).ok);
    auto r = reg.Invoke("crop.plant", {{"tileX", 0}, {"tileY", 0}, {"cropId", "parsnip"}},
                        CallerRole::Automation, ctx);
    CHECK_FALSE(r.ok);
    CHECK(r.code == ToolErrorCode::PreconditionFailed);
}

TEST_CASE("CropTools:crop.plant 未知作物 PreconditionFailed") {
    ToolRegistry reg;
    reg.Register(MakeCropPlantTool());
    me::scene::Scene scene;
    me::command::CommandStack stack;
    ToolInvocationLog log;
    auto farm = MakeFarm();
    ToolContext ctx{scene, stack, log, nullptr, &farm};

    auto r = reg.Invoke("crop.plant", {{"tileX", 0}, {"tileY", 0}, {"cropId", "banana"}},
                        CallerRole::Automation, ctx);
    CHECK_FALSE(r.ok);
    CHECK(r.code == ToolErrorCode::PreconditionFailed);
}

TEST_CASE("CropTools:crop.plant dry-run 零副作用") {
    ToolRegistry reg;
    reg.Register(MakeCropPlantTool());
    reg.Register(MakeCropGetFieldTool());
    me::scene::Scene scene;
    me::command::CommandStack stack;
    ToolInvocationLog log;
    auto farm = MakeFarm();
    ToolContext ctx{scene, stack, log, nullptr, &farm};

    auto r = reg.Invoke("crop.plant", {{"tileX", 1}, {"tileY", 1}, {"cropId", "parsnip"}},
                        CallerRole::Automation, ctx, /*dryRun=*/true);
    REQUIRE(r.ok);
    CHECK(farm.At(1, 1) == nullptr); // 真身未变
}

TEST_CASE("CropTools:crop.water 浇水标记") {
    ToolRegistry reg;
    reg.Register(MakeCropPlantTool());
    reg.Register(MakeCropWaterTool());
    me::scene::Scene scene;
    me::command::CommandStack stack;
    ToolInvocationLog log;
    auto farm = MakeFarm();
    ToolContext ctx{scene, stack, log, nullptr, &farm};

    REQUIRE(reg.Invoke("crop.plant", {{"tileX", 0}, {"tileY", 0}, {"cropId", "parsnip"}},
                       CallerRole::Automation, ctx).ok);
    auto r = reg.Invoke("crop.water", {{"tileX", 0}, {"tileY", 0}}, CallerRole::Automation, ctx);
    REQUIRE(r.ok);
    CHECK(r.data["watered"] == true);
}

TEST_CASE("CropTools:crop.water 空瓦片 PreconditionFailed") {
    ToolRegistry reg;
    reg.Register(MakeCropWaterTool());
    me::scene::Scene scene;
    me::command::CommandStack stack;
    ToolInvocationLog log;
    auto farm = MakeFarm();
    ToolContext ctx{scene, stack, log, nullptr, &farm};

    auto r = reg.Invoke("crop.water", {{"tileX", 9}, {"tileY", 9}}, CallerRole::Automation, ctx);
    CHECK_FALSE(r.ok);
    CHECK(r.code == ToolErrorCode::PreconditionFailed);
}

TEST_CASE("CropTools:crop.get_field 无农田 PreconditionFailed") {
    ToolRegistry reg;
    reg.Register(MakeCropGetFieldTool());
    me::scene::Scene scene;
    me::command::CommandStack stack;
    ToolInvocationLog log;
    ToolContext ctx{scene, stack, log}; // farm = nullptr

    auto r = reg.Invoke("crop.get_field", nlohmann::json::object(), CallerRole::Agent, ctx);
    CHECK_FALSE(r.ok);
    CHECK(r.code == ToolErrorCode::PreconditionFailed);
}

TEST_CASE("CropTools:crop.plant schema 拒绝缺字段") {
    ToolRegistry reg;
    reg.Register(MakeCropPlantTool());
    me::scene::Scene scene;
    me::command::CommandStack stack;
    ToolInvocationLog log;
    auto farm = MakeFarm();
    ToolContext ctx{scene, stack, log, nullptr, &farm};

    auto r = reg.Invoke("crop.plant", {{"tileX", 0}, {"tileY", 0}},
                        CallerRole::Automation, ctx);
    CHECK_FALSE(r.ok);
    CHECK(r.code == ToolErrorCode::InvalidParams);
}

TEST_CASE("CropTools:crop.advance_days 推进并回报 advanced") {
    ToolRegistry reg;
    reg.Register(MakeCropPlantTool());
    reg.Register(MakeCropWaterTool());
    reg.Register(MakeCropAdvanceDaysTool());
    me::scene::Scene scene;
    me::command::CommandStack stack;
    ToolInvocationLog log;
    auto farm = MakeFarm();
    ToolContext ctx{scene, stack, log, nullptr, &farm};

    REQUIRE(reg.Invoke("crop.plant", {{"tileX", 0}, {"tileY", 0}, {"cropId", "parsnip"}},
                       CallerRole::Automation, ctx).ok);
    REQUIRE(reg.Invoke("crop.water", {{"tileX", 0}, {"tileY", 0}},
                       CallerRole::Automation, ctx).ok);
    auto r = reg.Invoke("crop.advance_days", {{"days", 1}}, CallerRole::Automation, ctx);
    REQUIRE(r.ok);
    CHECK(r.data["advanced"] == 1);
    CHECK(r.data["crops"][0]["stage"] == 1);
    CHECK(farm.At(0, 0)->stage == 1); // 真实状态推进
}

TEST_CASE("CropTools:crop.advance_days dry-run 零副作用") {
    ToolRegistry reg;
    reg.Register(MakeCropPlantTool());
    reg.Register(MakeCropWaterTool());
    reg.Register(MakeCropAdvanceDaysTool());
    me::scene::Scene scene;
    me::command::CommandStack stack;
    ToolInvocationLog log;
    auto farm = MakeFarm();
    ToolContext ctx{scene, stack, log, nullptr, &farm};

    REQUIRE(reg.Invoke("crop.plant", {{"tileX", 0}, {"tileY", 0}, {"cropId", "parsnip"}},
                       CallerRole::Automation, ctx).ok);
    REQUIRE(reg.Invoke("crop.water", {{"tileX", 0}, {"tileY", 0}},
                       CallerRole::Automation, ctx).ok);
    auto r = reg.Invoke("crop.advance_days", {{"days", 1}}, CallerRole::Automation, ctx,
                        /*dryRun=*/true);
    REQUIRE(r.ok);
    CHECK(r.data["crops"][0]["stage"] == 1); // 预览前进
    CHECK(farm.At(0, 0)->stage == 0);         // 真身未变
}

TEST_CASE("CropTools:crop.advance_days schema 拒绝 days<1") {
    ToolRegistry reg;
    reg.Register(MakeCropAdvanceDaysTool());
    me::scene::Scene scene;
    me::command::CommandStack stack;
    ToolInvocationLog log;
    auto farm = MakeFarm();
    ToolContext ctx{scene, stack, log, nullptr, &farm};

    auto r = reg.Invoke("crop.advance_days", {{"days", 0}}, CallerRole::Automation, ctx);
    CHECK_FALSE(r.ok);
    CHECK(r.code == ToolErrorCode::InvalidParams);
}

TEST_CASE("CropTools:crop.harvest 成熟收获产出") {
    ToolRegistry reg;
    reg.Register(MakeCropPlantTool());
    reg.Register(MakeCropWaterTool());
    reg.Register(MakeCropAdvanceDaysTool());
    reg.Register(MakeCropHarvestTool());
    me::scene::Scene scene;
    me::command::CommandStack stack;
    ToolInvocationLog log;
    auto farm = MakeFarm();
    ToolContext ctx{scene, stack, log, nullptr, &farm};

    REQUIRE(reg.Invoke("crop.plant", {{"tileX", 0}, {"tileY", 0}, {"cropId", "parsnip"}},
                       CallerRole::Automation, ctx).ok);
    for (int day = 0; day < 3; ++day) { // 推到成熟(stage 3)
        REQUIRE(reg.Invoke("crop.water", {{"tileX", 0}, {"tileY", 0}},
                           CallerRole::Automation, ctx).ok);
        REQUIRE(reg.Invoke("crop.advance_days", {{"days", 1}},
                           CallerRole::Automation, ctx).ok);
    }
    auto r = reg.Invoke("crop.harvest", {{"tileX", 0}, {"tileY", 0}}, CallerRole::Editor, ctx);
    REQUIRE(r.ok);
    CHECK(r.data["itemId"] == "parsnip");
    CHECK(r.data["count"] == 1);
    CHECK(farm.At(0, 0) == nullptr); // 瓦片已清空
}

TEST_CASE("CropTools:crop.harvest 未成熟 PreconditionFailed") {
    ToolRegistry reg;
    reg.Register(MakeCropPlantTool());
    reg.Register(MakeCropHarvestTool());
    me::scene::Scene scene;
    me::command::CommandStack stack;
    ToolInvocationLog log;
    auto farm = MakeFarm();
    ToolContext ctx{scene, stack, log, nullptr, &farm};

    REQUIRE(reg.Invoke("crop.plant", {{"tileX", 0}, {"tileY", 0}, {"cropId", "parsnip"}},
                       CallerRole::Automation, ctx).ok);
    auto r = reg.Invoke("crop.harvest", {{"tileX", 0}, {"tileY", 0}}, CallerRole::Editor, ctx);
    CHECK_FALSE(r.ok);
    CHECK(r.code == ToolErrorCode::PreconditionFailed);
}

TEST_CASE("CropTools:crop.harvest 权限——Agent/Automation 被拒") {
    ToolRegistry reg;
    reg.Register(MakeCropHarvestTool());
    me::scene::Scene scene;
    me::command::CommandStack stack;
    ToolInvocationLog log;
    auto farm = MakeFarm();
    ToolContext ctx{scene, stack, log, nullptr, &farm};

    auto a = reg.Invoke("crop.harvest", {{"tileX", 0}, {"tileY", 0}}, CallerRole::Agent, ctx);
    CHECK(a.code == ToolErrorCode::PermissionDenied);
    auto m = reg.Invoke("crop.harvest", {{"tileX", 0}, {"tileY", 0}}, CallerRole::Automation, ctx);
    CHECK(m.code == ToolErrorCode::PermissionDenied); // EditorOnly
}
