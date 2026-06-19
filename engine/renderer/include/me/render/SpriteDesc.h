#pragma once

#include "me/core/Rect.h"
#include "me/core/Vector4.h"

namespace me::rhi { class GpuTexture; }

namespace me::render {

/**
 * @brief 一次精灵绘制的提交单元(POD)。提交给 SpriteBatch::Submit。
 *
 * srcRect:纹理中的采样子区域,归一化 UV(x=uMin,y=vMin(上),width/height 为跨度);
 *          整贴图传 {0,0,1,1}。纹理 V 向下。
 * dstRect:目标世界矩形,像素单位,原点左下、Y 向上(x,y=左下角,width/height=尺寸)。
 * color:RGBA 线性色调,与采样结果相乘;整色传 {1,1,1,1}。
 * rotation:绕 dstRect 中心的弧度旋转。
 * texture:非拥有指针,生命周期归调用方(M2 暂不经 AssetManager)。
 */
struct SpriteDesc {
    const me::rhi::GpuTexture* texture = nullptr;
    me::Rect srcRect{0.0f, 0.0f, 1.0f, 1.0f};
    me::Rect dstRect{};
    me::Vector4 color{1.0f, 1.0f, 1.0f, 1.0f};
    float rotation = 0.0f;
};

} // namespace me::render
