#pragma once

#include "me/core/Matrix4x4.h"
#include "me/core/Vector2.h"

namespace me::rhi {

/**
 * @brief 由位置/尺寸/旋转构造精灵的世界模型矩阵(行向量约定 v' = v*M)。
 *
 * 顺序:先缩放到尺寸,再旋转,最后平移到位置 —— model = S * R * T。
 * 单位四边形角点 (±0.5) 经此矩阵落到世界矩形 [pos - size/2, pos + size/2]。
 */
inline me::Matrix4x4 MakeSpriteModelMatrix(me::Vector2 position,
                                           me::Vector2 size,
                                           float rotationRadians) {
    const me::Matrix4x4 s = me::Matrix4x4::Scale(size);
    const me::Matrix4x4 r = me::Matrix4x4::Rotation(rotationRadians);
    const me::Matrix4x4 t = me::Matrix4x4::Translation(position);
    return s * r * t; // 行向量:v * S * R * T
}

} // namespace me::rhi
