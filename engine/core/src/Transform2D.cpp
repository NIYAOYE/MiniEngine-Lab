#include "me/core/Transform2D.h"
#include "me/core/Matrix4x4.h"

namespace me {

Matrix4x4 Transform2D::ToMatrix() const {
    // 行向量约定:p' = p * S * R * T(先缩放、再旋转、后平移)
    return Matrix4x4::Scale(scale)
         * Matrix4x4::Rotation(rotation)
         * Matrix4x4::Translation(position);
}

} // namespace me
