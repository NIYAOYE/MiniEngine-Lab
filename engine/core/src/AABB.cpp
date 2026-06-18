#include "me/core/AABB.h"

#include <algorithm>

namespace me {

bool AABB::Contains(const Vector2& p) const {
    return p.x >= min.x && p.x <= max.x
        && p.y >= min.y && p.y <= max.y;
}

bool AABB::Intersects(const AABB& other) const {
    return min.x <= other.max.x && max.x >= other.min.x
        && min.y <= other.max.y && max.y >= other.min.y;
}

AABB AABB::Expanded(const Vector2& p) const {
    AABB r;
    r.min = Vector2{std::min(min.x, p.x), std::min(min.y, p.y)};
    r.max = Vector2{std::max(max.x, p.x), std::max(max.y, p.y)};
    return r;
}

} // namespace me
