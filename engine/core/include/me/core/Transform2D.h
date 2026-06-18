#pragma once

#include "me/core/Vector2.h"

namespace me {

struct Matrix4x4;

/**
 * @brief 2D 变换(纯值类型):位置 / 旋转(弧度)/ 缩放。
 *
 * 父子层级与脏标记由 Scene 层(M4)负责,本类型只描述单个局部变换。
 */
struct Transform2D {
    Vector2 position{0.0f, 0.0f};
    float   rotation = 0.0f;        ///< 弧度,绕 +Z 逆时针
    Vector2 scale{1.0f, 1.0f};

    /** @brief 生成局部→父空间矩阵(行向量:Scale * Rotation * Translation)。 */
    Matrix4x4 ToMatrix() const;
};

} // namespace me
