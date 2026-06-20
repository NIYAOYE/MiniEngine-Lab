#include "me/scene/TransformSystem.h"

#include "me/scene/Scene.h"

namespace me::scene {

void TransformSystem::UpdateWorldTransforms(Scene& scene) {
    // WorldMatrix 惰性递归解析父链并缓存;遍历全部存活实体即可全部解析。
    for (const Entity e : scene.AliveEntities()) {
        scene.WorldMatrix(e);
    }
}

} // namespace me::scene
