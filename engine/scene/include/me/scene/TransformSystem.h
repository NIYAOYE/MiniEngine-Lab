#pragma once

namespace me::scene {

class Scene;

/**
 * @brief 变换系统:批量解析全部存活实体的世界矩阵并清除脏标记。
 *
 * 无状态(静态方法),显式接收 Scene&(禁全局状态)。内部依赖 Scene::WorldMatrix
 * 的惰性父先于子解析,因此对任意遍历顺序都正确。
 */
class TransformSystem {
public:
    /// @brief 解析所有脏实体的世界矩阵(幂等:已是干净的实体零开销)。
    static void UpdateWorldTransforms(Scene& scene);
};

} // namespace me::scene
