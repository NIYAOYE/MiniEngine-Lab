#include "me/core/Matrix4x4.h"
#include "me/core/Vector2.h"
#include "me/core/Assert.h"

#include <cmath>

namespace me {

Matrix4x4 Matrix4x4::Identity() {
    Matrix4x4 r{};
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            r.m[i][j] = (i == j) ? 1.0f : 0.0f;
        }
    }
    return r;
}

Matrix4x4 Matrix4x4::Translation(const Vector2& t) {
    Matrix4x4 r = Identity();
    r.m[3][0] = t.x; // 行向量约定:平移在第 4 行
    r.m[3][1] = t.y;
    return r;
}

Matrix4x4 Matrix4x4::Scale(const Vector2& s) {
    Matrix4x4 r = Identity();
    r.m[0][0] = s.x;
    r.m[1][1] = s.y;
    return r;
}

Matrix4x4 Matrix4x4::Rotation(float radians) {
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    Matrix4x4 r = Identity();
    // 行向量绕 +Z 逆时针:[1,0]*R = [cos, sin]
    r.m[0][0] = c;  r.m[0][1] = s;
    r.m[1][0] = -s; r.m[1][1] = c;
    return r;
}

Matrix4x4 Matrix4x4::Orthographic(float left, float right, float bottom, float top,
                                  float nearZ, float farZ) {
    const float rl = right - left;
    const float tb = top - bottom;
    const float fn = farZ - nearZ;

    // 不变量:视口宽/高/深必须非零,否则是上层配置错误(会产生 inf/nan 矩阵)
    ME_ASSERT_MSG(rl != 0.0f, "Orthographic: left 与 right 不能相等");
    ME_ASSERT_MSG(tb != 0.0f, "Orthographic: bottom 与 top 不能相等");
    ME_ASSERT_MSG(fn != 0.0f, "Orthographic: nearZ 与 farZ 不能相等");

    Matrix4x4 r{}; // 全零起步
    r.m[0][0] = 2.0f / rl;
    r.m[1][1] = 2.0f / tb;
    r.m[2][2] = 1.0f / fn;
    r.m[3][0] = -(right + left) / rl;
    r.m[3][1] = -(top + bottom) / tb;
    r.m[3][2] = -nearZ / fn;
    r.m[3][3] = 1.0f;
    return r;
}

Matrix4x4 Matrix4x4::operator*(const Matrix4x4& rhs) const {
    Matrix4x4 r{};
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) {
                sum += m[i][k] * rhs.m[k][j];
            }
            r.m[i][j] = sum;
        }
    }
    return r;
}

Vector2 Matrix4x4::TransformPoint(const Vector2& p) const {
    // [x y 0 1] * M ,取 x/y 分量
    const float x = p.x * m[0][0] + p.y * m[1][0] + m[3][0];
    const float y = p.x * m[0][1] + p.y * m[1][1] + m[3][1];
    return Vector2{x, y};
}

Vector2 Matrix4x4::TransformVector(const Vector2& v) const {
    // [x y 0 0] * M ,无平移项
    const float x = v.x * m[0][0] + v.y * m[1][0];
    const float y = v.x * m[0][1] + v.y * m[1][1];
    return Vector2{x, y};
}

} // namespace me
