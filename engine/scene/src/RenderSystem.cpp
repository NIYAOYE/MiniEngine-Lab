#include "me/scene/RenderSystem.h"

#include <algorithm>
#include <cmath>

#include "me/scene/Scene.h"
#include "me/scene/Components.h"
#include "me/core/Matrix4x4.h"
#include "me/core/Vector2.h"

namespace me::scene {

namespace {

// 从 2D 世界矩阵分解出平移、旋转(弧度)、缩放(用基向量长度/方向)。
struct World2D {
    me::Vector2 position;
    float rotation;
    me::Vector2 scale;
};

World2D DecomposeWorld(const me::Matrix4x4& m) {
    World2D w;
    w.position = m.TransformPoint(me::Vector2{0.0f, 0.0f});
    const me::Vector2 right = m.TransformVector(me::Vector2{1.0f, 0.0f});
    const me::Vector2 up = m.TransformVector(me::Vector2{0.0f, 1.0f});
    w.scale = me::Vector2{right.Length(), up.Length()};
    w.rotation = std::atan2(right.y, right.x);
    return w;
}

} // namespace

RenderView RenderSystem::BuildRenderView(Scene& scene) {
    ComponentStorage<SpriteComponent>& store = scene.ComponentStore<SpriteComponent>();
    const std::vector<Entity>& owners = store.Entities();
    const std::vector<SpriteComponent>& sprites = store.Items();

    RenderView view;
    view.reserve(sprites.size());
    for (std::size_t i = 0; i < sprites.size(); ++i) {
        const Entity e = owners[i];
        if (!scene.IsAlive(e)) continue; // 防御:存储应已随销毁清理
        const SpriteComponent& sp = sprites[i];
        const World2D w = DecomposeWorld(scene.WorldMatrix(e));

        RenderItem item;
        item.textureId = sp.textureId;
        item.srcRect = sp.srcRect;
        item.color = sp.color;
        item.rotation = w.rotation;
        item.sortLayer = sp.sortLayer;

        // 世界像素尺寸 = 精灵 size × 世界缩放;按锚点摆放到世界位置。
        const float dstW = sp.size.x * w.scale.x;
        const float dstH = sp.size.y * w.scale.y;
        item.dstRect = me::Rect{
            w.position.x - sp.pivot.x * dstW,
            w.position.y - sp.pivot.y * dstH,
            dstW, dstH,
        };
        view.push_back(item);
    }

    // 稳定排序:层升序为主;同层世界 Y 降序(低 Y 后提交 → 画在最上,2.5D 叠压)。
    std::stable_sort(view.begin(), view.end(),
        [](const RenderItem& a, const RenderItem& b) {
            if (a.sortLayer != b.sortLayer) return a.sortLayer < b.sortLayer;
            // dstRect.y 为左下角;以其作为同层 Y 比较键即可(尺寸一致时等价于中心)。
            return a.dstRect.y > b.dstRect.y;
        });
    return view;
}

} // namespace me::scene
