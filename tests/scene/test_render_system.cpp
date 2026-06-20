#include <doctest/doctest.h>

#include "me/scene/Scene.h"
#include "me/scene/Components.h"
#include "me/scene/RenderSystem.h"
#include "me/scene/TransformSystem.h"

using me::scene::Entity;
using me::scene::Scene;
using me::scene::SpriteComponent;
using me::scene::RenderSystem;
using me::scene::RenderView;
using me::scene::TransformSystem;

namespace {
constexpr float kEps = 1e-4f;

// 在世界 (x,y) 放一个 size×size、锚点中心、指定层的精灵实体。
Entity MakeSprite(Scene& s, float x, float y, float size, int layer,
                  std::uint32_t tex = 0) {
    const Entity e = s.CreateEntity();
    me::Transform2D t;
    t.position = me::Vector2{x, y};
    s.SetLocalTransform(e, t);
    SpriteComponent sp;
    sp.textureId = tex;
    sp.size = me::Vector2{size, size};
    sp.sortLayer = layer;
    s.AddComponent<SpriteComponent>(e, sp);
    return e;
}
} // namespace

TEST_CASE("RenderSystem:dstRect 由世界位置 + size + 中心锚点导出") {
    Scene scene;
    MakeSprite(scene, 100.0f, 50.0f, 16.0f, 0);
    TransformSystem::UpdateWorldTransforms(scene);
    const RenderView view = RenderSystem::BuildRenderView(scene);
    REQUIRE(view.size() == 1);
    // 锚点中心:dst 左下 = (100 - 8, 50 - 8)。
    CHECK(view[0].dstRect.x == doctest::Approx(92.0f).epsilon(kEps));
    CHECK(view[0].dstRect.y == doctest::Approx(42.0f).epsilon(kEps));
    CHECK(view[0].dstRect.width == doctest::Approx(16.0f).epsilon(kEps));
    CHECK(view[0].dstRect.height == doctest::Approx(16.0f).epsilon(kEps));
}

TEST_CASE("RenderSystem:先按层升序排序") {
    Scene scene;
    MakeSprite(scene, 0.0f, 0.0f, 8.0f, /*layer*/2, /*tex*/20);
    MakeSprite(scene, 0.0f, 0.0f, 8.0f, /*layer*/0, /*tex*/10);
    MakeSprite(scene, 0.0f, 0.0f, 8.0f, /*layer*/1, /*tex*/15);
    TransformSystem::UpdateWorldTransforms(scene);
    const RenderView view = RenderSystem::BuildRenderView(scene);
    REQUIRE(view.size() == 3);
    CHECK(view[0].sortLayer == 0);
    CHECK(view[1].sortLayer == 1);
    CHECK(view[2].sortLayer == 2);
}

TEST_CASE("RenderSystem:同层内按世界 Y 降序(高 Y 在前,低 Y 压在上)") {
    Scene scene;
    MakeSprite(scene, 0.0f, /*y*/10.0f, 8.0f, 0, /*tex*/1);
    MakeSprite(scene, 0.0f, /*y*/90.0f, 8.0f, 0, /*tex*/2);
    MakeSprite(scene, 0.0f, /*y*/50.0f, 8.0f, 0, /*tex*/3);
    TransformSystem::UpdateWorldTransforms(scene);
    const RenderView view = RenderSystem::BuildRenderView(scene);
    REQUIRE(view.size() == 3);
    // Y 降序:90 → 50 → 10(低 Y 最后提交 = 画在最上,符合 2.5D)。
    CHECK(view[0].dstRect.y > view[1].dstRect.y);
    CHECK(view[1].dstRect.y > view[2].dstRect.y);
    CHECK(view[0].textureId == 2);
    CHECK(view[2].textureId == 1);
}

TEST_CASE("RenderSystem:无 SpriteComponent 的实体被忽略") {
    Scene scene;
    scene.CreateEntity(); // 无精灵
    MakeSprite(scene, 0.0f, 0.0f, 8.0f, 0);
    TransformSystem::UpdateWorldTransforms(scene);
    CHECK(RenderSystem::BuildRenderView(scene).size() == 1);
}

TEST_CASE("RenderSystem:srcRect/color/textureId 透传") {
    Scene scene;
    const Entity e = scene.CreateEntity();
    scene.SetLocalTransform(e, me::Transform2D{});
    SpriteComponent sp;
    sp.textureId = 7;
    sp.srcRect = me::Rect{0.25f, 0.5f, 0.25f, 0.25f};
    sp.color = me::Vector4{0.1f, 0.2f, 0.3f, 0.4f};
    sp.size = me::Vector2{8.0f, 8.0f};
    scene.AddComponent<SpriteComponent>(e, sp);
    TransformSystem::UpdateWorldTransforms(scene);
    const RenderView view = RenderSystem::BuildRenderView(scene);
    REQUIRE(view.size() == 1);
    CHECK(view[0].textureId == 7);
    CHECK(view[0].srcRect.x == doctest::Approx(0.25f).epsilon(kEps));
    CHECK(view[0].color.y == doctest::Approx(0.2f).epsilon(kEps));
}
