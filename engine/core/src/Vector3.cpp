#include "me/core/Vector3.h"

#include <cmath>

namespace me {

float Vector3::Length() const {
    return std::sqrt(LengthSquared());
}

} // namespace me
