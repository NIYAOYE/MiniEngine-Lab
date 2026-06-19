#pragma once

#include <array>
#include <cstdint>

namespace me::rhi {

/// 精灵顶点:局部坐标 (x,y) + 纹理坐标 (u,v)。布局须与 sprite.hlsl 的输入一致。
struct SpriteVertex {
    float x, y; // 局部空间,单位四边形居中于原点,角点在 ±0.5
    float u, v; // 纹理坐标,左上 (0,0) 右下 (1,1)
};

constexpr uint32_t kSpriteVertexCount = 4;
constexpr uint32_t kSpriteIndexCount = 6;

/**
 * @brief 居中单位四边形的 4 个顶点(局部 ±0.5)。
 *
 * 世界 Y 向上,纹理 V 向下:故局部底边 (y=-0.5) 对应 v=1,顶边 (y=+0.5) 对应 v=0,
 * 使贴图正立显示。索引顺序见 UnitQuadIndices;M1 关闭背面剔除,绕序无关。
 */
inline std::array<SpriteVertex, kSpriteVertexCount> UnitQuadVertices() {
    return {{
        {-0.5f, -0.5f, 0.0f, 1.0f}, // 0 左下
        { 0.5f, -0.5f, 1.0f, 1.0f}, // 1 右下
        { 0.5f,  0.5f, 1.0f, 0.0f}, // 2 右上
        {-0.5f,  0.5f, 0.0f, 0.0f}, // 3 左上
    }};
}

/** @brief 两个三角形:0-1-2 与 0-2-3。 */
inline std::array<uint16_t, kSpriteIndexCount> UnitQuadIndices() {
    return {{0, 1, 2, 0, 2, 3}};
}

} // namespace me::rhi
