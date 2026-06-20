#pragma once

#include <cstdint>

#include "me/core/Rect.h"
#include "me/core/Vector2.h"
#include "me/core/Vector4.h"

namespace me::scene {

/// 精灵锚点默认值:0.5 = 中心(与 SpriteDesc 绕中心旋转一致)。
constexpr float kDefaultPivot = 0.5f;

/// 无纹理引用哨兵(textureId 的无效值)。
constexpr std::uint32_t kInvalidTextureId = 0xFFFFFFFFu;

/**
 * @brief 精灵渲染组件(纯数据)。纹理用 RHI 无关的 textureId 引用,
 *        在渲染边界(sandbox/Engine)解析为实际 GpuTexture*,保持 Scene 与 RHI 解耦。
 *
 * size:世界像素尺寸;pivot:归一化锚点(0..1),决定 size 相对实体世界位置的摆放;
 * sortLayer:主排序键(小在底);同层内按世界 Y 降序(高 Y 在后,低 Y 压在上,2.5D)。
 */
struct SpriteComponent {
    std::uint32_t textureId = kInvalidTextureId;
    me::Rect srcRect{0.0f, 0.0f, 1.0f, 1.0f};
    me::Vector2 size{0.0f, 0.0f};
    me::Vector2 pivot{kDefaultPivot, kDefaultPivot};
    me::Vector4 color{1.0f, 1.0f, 1.0f, 1.0f};
    int sortLayer = 0;
};

/**
 * @brief 正交相机组件(纯数据)。相机中心 = 实体世界位置;zoom>1 放大。
 *        viewportSize 为像素视口尺寸(通常等于渲染目标尺寸)。
 */
struct CameraComponent {
    float zoom = 1.0f;
    me::Vector2 viewportSize{0.0f, 0.0f};
};

} // namespace me::scene
