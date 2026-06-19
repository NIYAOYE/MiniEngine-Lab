#pragma once

#include <cstdint>
#include <vector>
#include <d3d12.h>

namespace me::rhi {

class Fence;

/**
 * @brief 把 RGBA8 纹理同步回读为紧凑(无行填充)CPU 像素。仅供测试/调试。
 *
 * beforeState 是纹理当前所处状态(回读前转成 COPY_SOURCE,回读后转回)。
 */
std::vector<uint8_t> ReadbackRgba8(ID3D12Device* device,
                                   ID3D12CommandQueue* queue,
                                   Fence& fence,
                                   ID3D12Resource* tex,
                                   uint32_t width, uint32_t height,
                                   D3D12_RESOURCE_STATES beforeState);

} // namespace me::rhi
