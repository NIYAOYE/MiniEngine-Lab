#pragma once

#include <cstdint>
#include <vector>

#include "me/core/Rect.h"
#include "me/core/Vector4.h"

namespace me::scene {

/**
 * @brief 一条渲染指令(纯数据,RHI 无关)。RenderSystem 产出,渲染边界消费:
 *        把 textureId 解析为 GpuTexture* 后填入 SpriteDesc 提交给 SpriteBatch。
 *
 * dstRect:世界像素矩形(左下原点、Y 向上);srcRect:归一化 UV;rotation:绕中心弧度。
 */
struct RenderItem {
    std::uint32_t textureId = 0;
    me::Rect srcRect{0.0f, 0.0f, 1.0f, 1.0f};
    me::Rect dstRect{};
    me::Vector4 color{1.0f, 1.0f, 1.0f, 1.0f};
    float rotation = 0.0f;
    int sortLayer = 0;
};

/// 一帧的可见渲染指令序列,已按(层升序、世界 Y 降序)稳定排序。
using RenderView = std::vector<RenderItem>;

} // namespace me::scene
