#pragma once

#include <cstddef>
#include <memory>
#include <d3d12.h>

#include "me/rhi/D3DCommon.h"

namespace me::rhi {

/**
 * @brief 上传堆缓冲:CPU 可写、GPU 可读。M1 用于顶点/索引缓冲。
 *
 * 性能审查点:上传堆位于系统内存,GPU 每次读取需过 PCIe。生产应改用默认堆 +
 * 一次性拷贝。M1 以可读性优先,先这样。
 */
class GpuBuffer {
public:
    static std::unique_ptr<GpuBuffer> CreateUpload(ID3D12Device* device,
                                                   const void* data,
                                                   size_t sizeBytes);

    D3D12_GPU_VIRTUAL_ADDRESS Gpu() const { return m_resource->GetGPUVirtualAddress(); }
    size_t Size() const { return m_size; }
    ID3D12Resource* Resource() const { return m_resource.Get(); }

private:
    GpuBuffer() = default;
    ComPtr<ID3D12Resource> m_resource;
    size_t m_size = 0;
};

} // namespace me::rhi
