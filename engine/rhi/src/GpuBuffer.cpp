#include "me/rhi/GpuBuffer.h"

#include <cstring>

namespace me::rhi {

std::unique_ptr<GpuBuffer> GpuBuffer::CreateUpload(ID3D12Device* device,
                                                   const void* data,
                                                   size_t sizeBytes) {
    auto self = std::unique_ptr<GpuBuffer>(new GpuBuffer());
    self->m_size = sizeBytes;

    D3D12_HEAP_PROPERTIES heap = {};
    heap.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = sizeBytes;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    if (FAILED(device->CreateCommittedResource(
            &heap, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&self->m_resource)))) {
        return nullptr;
    }

    // 映射并拷入数据(上传堆 CPU 可见)。
    void* mapped = nullptr;
    D3D12_RANGE noRead = {0, 0}; // CPU 不读
    if (FAILED(self->m_resource->Map(0, &noRead, &mapped))) {
        return nullptr;
    }
    std::memcpy(mapped, data, sizeBytes);
    self->m_resource->Unmap(0, nullptr);
    return self;
}

} // namespace me::rhi
