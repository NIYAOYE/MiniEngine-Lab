#include "me/core/Vector2.h"
#include "me/core/MathConstants.h"

#include <cmath>

namespace me {

float Vector2::Length() const {
    return std::sqrt(LengthSquared());
}

Vector2 Vector2::Normalized() const {
    const float len = Length();
    if (len <= kEpsilon) {
        return Vector2{0.0f, 0.0f}; // 防除零:零向量归一化仍为零向量
    }
    const float inv = 1.0f / len;
    return Vector2{x * inv, y * inv};
}

} // namespace me
