#pragma once

#include <cstdint>
#include <memory>
#include <d3d12.h>

#include "me/rhi/D3DCommon.h"
#include "me/rhi/DescriptorHeap.h"

namespace me::rhi {

class Fence;

/// 精灵纹理像素格式(具名常量)。
constexpr DXGI_FORMAT kSpriteTextureFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

/**
 * @brief 默认堆 2D 纹理:同步上传 RGBA8 像素、转到 PIXEL_SHADER_RESOURCE、建 SRV。
 *
 * 接收裸像素指针(不依赖 Assets 层);上传在 Create 内同步完成(Fence 刷队列)。
 */
class GpuTexture {
public:
    static std::unique_ptr<GpuTexture> Create(ID3D12Device* device,
                                              ID3D12CommandQueue* queue,
                                              Fence& fence,
                                              uint32_t width, uint32_t height,
                                              const uint8_t* rgba8,
                                              const Descriptor& srv);

    ID3D12Resource* Resource() const { return m_resource.Get(); }
    uint32_t Width() const { return m_width; }
    uint32_t Height() const { return m_height; }
    D3D12_GPU_DESCRIPTOR_HANDLE SrvGpu() const { return m_srvGpu; }

private:
    GpuTexture() = default;
    ComPtr<ID3D12Resource> m_resource;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    D3D12_GPU_DESCRIPTOR_HANDLE m_srvGpu{};
};

} // namespace me::rhi
