#pragma once

#include "me/core/Vector2.h"

namespace me {

/** @brief 轴对齐包围盒,由 min/max 两角定义。约定 min <= max(逐分量)。 */
struct AABB {
    Vector2 min{0.0f, 0.0f};
    Vector2 max{0.0f, 0.0f};

    /** @brief 中心点。 */
    Vector2 Center() const { return (min + max) * 0.5f; }
    /** @brief 半尺寸(extents)。 */
    Vector2 Extents() const { return (max - min) * 0.5f; }

    /** @brief 点是否在盒内(闭区间 [min, max])。 */
    bool Contains(const Vector2& p) const;
    /** @brief 与另一盒是否相交(含边接触)。 */
    bool Intersects(const AABB& other) const;
    /** @brief 返回把点 p 并入后的新包围盒。 */
    AABB Expanded(const Vector2& p) const;
};

} // namespace me
