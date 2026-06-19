#pragma once

#include "me/core/Matrix4x4.h"
#include "me/core/Vector2.h"
#include "me/core/Assert.h"

namespace me::render {

/**
 * @brief 2D 正交相机:由中心位置、缩放与视口尺寸生成世界→裁剪的 viewProj。
 *
 * 约定世界空间 Y 轴向上、像素为单位。position 是相机在世界中的中心点;
 * zoom>1 放大(可见范围按比例缩小)。ViewProj() 直接复用 Matrix4x4::Orthographic,
 * 不单独维护 view 矩阵 —— 把可见矩形 [pos±halfExtent] 映射到 NDC。
 */
class OrthographicCamera {
public:
    /// @brief 设置视口像素尺寸(通常等于窗口/渲染目标尺寸)。
    void SetViewportSize(float width, float height) {
        m_width = width;
        m_height = height;
    }
    /// @brief 设置相机中心(世界像素坐标)。
    void SetPosition(const Vector2& position) { m_position = position; }
    /// @brief 设置缩放(>0;1=一屏一比一,>1 放大)。
    void SetZoom(float zoom) { m_zoom = zoom; }

    const Vector2& Position() const { return m_position; }
    float Zoom() const { return m_zoom; }

    /// @brief 生成世界→NDC 的正交矩阵(行向量约定,v' = v * viewProj)。
    Matrix4x4 ViewProj() const {
        ME_ASSERT_MSG(m_zoom > 0.0f, "OrthographicCamera: zoom 必须为正");
        ME_ASSERT_MSG(m_width > 0.0f && m_height > 0.0f,
                      "OrthographicCamera: 视口尺寸必须为正");
        const float halfW = (m_width * 0.5f) / m_zoom;
        const float halfH = (m_height * 0.5f) / m_zoom;
        return Matrix4x4::Orthographic(m_position.x - halfW, m_position.x + halfW,
                                       m_position.y - halfH, m_position.y + halfH,
                                       kNearZ, kFarZ);
    }

private:
    static constexpr float kNearZ = 0.0f;
    static constexpr float kFarZ = 1.0f;
    Vector2 m_position{0.0f, 0.0f};
    float m_zoom = 1.0f;
    float m_width = 1.0f;
    float m_height = 1.0f;
};

} // namespace me::render
