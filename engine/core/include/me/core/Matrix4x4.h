#pragma once

namespace me {

struct Vector2;

/**
 * @brief 4x4 矩阵。
 *
 * 约定:**行主序存储**(m[row][col]),**行向量**乘法(v' = v * M),
 * 平移分量位于第 4 行(m[3][0..2])。与 DirectXMath/HLSL 同源,
 * 便于 M1 起接入 DX12。坐标系:世界空间 Y 轴向上。
 */
struct Matrix4x4 {
    float m[4][4];

    /** @brief 单位矩阵。 */
    static Matrix4x4 Identity();
    /** @brief 平移矩阵。 */
    static Matrix4x4 Translation(const Vector2& t);
    /** @brief 缩放矩阵。 */
    static Matrix4x4 Scale(const Vector2& s);
    /** @brief 绕 +Z 轴逆时针旋转(弧度;Y 轴向上)。 */
    static Matrix4x4 Rotation(float radians);
    /**
     * @brief 左手正交投影(z 映射到 [0,1],适配 DX)。
     * 将 [left,right]x[bottom,top] 映射到 NDC [-1,1]x[-1,1]。
     */
    static Matrix4x4 Orthographic(float left, float right, float bottom, float top,
                                  float nearZ, float farZ);

    /** @brief 矩阵乘法(this * rhs)。 */
    Matrix4x4 operator*(const Matrix4x4& rhs) const;

    /** @brief 变换点(隐含 w=1,受平移影响)。 */
    Vector2 TransformPoint(const Vector2& p) const;
    /** @brief 变换方向向量(隐含 w=0,不受平移影响)。 */
    Vector2 TransformVector(const Vector2& v) const;
};

} // namespace me
