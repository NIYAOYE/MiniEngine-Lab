#pragma once

namespace me {

/** @brief 四维向量(主要用作 RGBA 颜色 / 齐次坐标)。 */
struct Vector4 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;

    constexpr Vector4() = default;
    constexpr Vector4(float xIn, float yIn, float zIn, float wIn)
        : x(xIn), y(yIn), z(zIn), w(wIn) {}

    Vector4 operator+(const Vector4& r) const { return {x + r.x, y + r.y, z + r.z, w + r.w}; }
    Vector4 operator-(const Vector4& r) const { return {x - r.x, y - r.y, z - r.z, w - r.w}; }
    Vector4 operator*(float s) const { return {x * s, y * s, z * s, w * s}; }
    bool operator==(const Vector4& r) const {
        return x == r.x && y == r.y && z == r.z && w == r.w;
    }
    bool operator!=(const Vector4& r) const { return !(*this == r); }
};

} // namespace me
