#pragma once

#include "me/core/Assert.h"

namespace me {

/**
 * @brief 二维向量(引擎主力数学类型)。
 *
 * 公开字段 x/y 为 POD,直接访问。约定世界空间 Y 轴向上。
 */
struct Vector2 {
    float x = 0.0f;
    float y = 0.0f;

    constexpr Vector2() = default;
    constexpr Vector2(float xIn, float yIn) : x(xIn), y(yIn) {}

    Vector2 operator+(const Vector2& r) const { return {x + r.x, y + r.y}; }
    Vector2 operator-(const Vector2& r) const { return {x - r.x, y - r.y}; }
    Vector2 operator*(float s) const { return {x * s, y * s}; }
    Vector2 operator/(float s) const {
        ME_ASSERT_MSG(s != 0.0f, "Vector2::operator/: 除数为 0");
        return {x / s, y / s};
    }
    Vector2 operator-() const { return {-x, -y}; }

    Vector2& operator+=(const Vector2& r) { x += r.x; y += r.y; return *this; }
    Vector2& operator-=(const Vector2& r) { x -= r.x; y -= r.y; return *this; }
    Vector2& operator*=(float s) { x *= s; y *= s; return *this; }

    bool operator==(const Vector2& r) const { return x == r.x && y == r.y; }
    bool operator!=(const Vector2& r) const { return !(*this == r); }

    /** @brief 长度平方(避免开方,用于比较)。 */
    float LengthSquared() const { return x * x + y * y; }
    /** @brief 欧氏长度。 */
    float Length() const;
    /** @brief 返回单位向量;长度近零时返回零向量(不除零)。 */
    Vector2 Normalized() const;
};

/** @brief 点积。 */
inline float Dot(const Vector2& a, const Vector2& b) { return a.x * b.x + a.y * b.y; }

} // namespace me
