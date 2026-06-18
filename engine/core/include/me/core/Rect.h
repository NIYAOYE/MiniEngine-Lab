#pragma once

#include "me/core/Vector2.h"

namespace me {

/** @brief 轴对齐矩形,左下角 (x,y) + 尺寸 (width,height)。约定 width/height >= 0。 */
struct Rect {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;

    /** @brief 左下角。 */
    Vector2 Min() const { return Vector2{x, y}; }
    /** @brief 右上角。 */
    Vector2 Max() const { return Vector2{x + width, y + height}; }

    /** @brief 点是否在矩形内(半开区间 [min, max))。 */
    bool Contains(const Vector2& p) const;
    /** @brief 与另一矩形是否相交(边接触视为不相交)。 */
    bool Intersects(const Rect& other) const;
};

} // namespace me
