#pragma once

#include "me/core/Vector2.h"

namespace me::scene {

/**
 * @brief 解析后的相机参数(纯数据,RHI/Renderer 无关)。
 *        渲染边界据此构造 me::render::OrthographicCamera。
 */
struct CameraView {
    me::Vector2 center{0.0f, 0.0f};
    float zoom = 1.0f;
    me::Vector2 viewportSize{0.0f, 0.0f};
};

} // namespace me::scene
