#include "me/core/Rect.h"

namespace me {

bool Rect::Contains(const Vector2& p) const {
    return p.x >= x && p.x < (x + width)
        && p.y >= y && p.y < (y + height);
}

bool Rect::Intersects(const Rect& other) const {
    return x < (other.x + other.width)
        && (x + width) > other.x
        && y < (other.y + other.height)
        && (y + height) > other.y;
}

} // namespace me
