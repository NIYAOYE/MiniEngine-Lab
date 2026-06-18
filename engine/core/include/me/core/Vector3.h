#pragma once

namespace me {

/** @brief 三维向量(用于齐次/方向;颜色亦可)。 */
struct Vector3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    constexpr Vector3() = default;
    constexpr Vector3(float xIn, float yIn, float zIn) : x(xIn), y(yIn), z(zIn) {}

    Vector3 operator+(const Vector3& r) const { return {x + r.x, y + r.y, z + r.z}; }
    Vector3 operator-(const Vector3& r) const { return {x - r.x, y - r.y, z - r.z}; }
    Vector3 operator*(float s) const { return {x * s, y * s, z * s}; }
    bool operator==(const Vector3& r) const { return x == r.x && y == r.y && z == r.z; }
    bool operator!=(const Vector3& r) const { return !(*this == r); }

    float LengthSquared() const { return x * x + y * y + z * z; }
    float Length() const;
};

/** @brief 点积。 */
inline float Dot(const Vector3& a, const Vector3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

} // namespace me
