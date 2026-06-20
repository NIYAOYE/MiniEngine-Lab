#pragma once

#include "me/scene/RenderView.h"

namespace me::scene {

class Scene;

/**
 * @brief 渲染系统:收集存活实体的 SpriteComponent,结合世界变换算 dstRect,
 *        产出按(层升序、世界 Y 降序)稳定排序的 RenderView。
 *
 * 纯数据输出,不碰 RHI/Renderer。调用前应先 TransformSystem::UpdateWorldTransforms。
 */
class RenderSystem {
public:
    static RenderView BuildRenderView(Scene& scene);
};

} // namespace me::scene
