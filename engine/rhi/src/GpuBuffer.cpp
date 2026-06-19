#include "me/rhi/GpuBuffer.h"

#include <cstring>
#include "me/core/Assert.h"

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

std::unique_ptr<GpuBuffer> GpuBuffer::CreateDynamic(ID3D12Device* device,
                                                    size_t capacityBytes) {
    auto self = std::unique_ptr<GpuBuffer>(new GpuBuffer());
    self->m_size = capacityBytes;

    D3D12_HEAP_PROPERTIES heap = {};
    heap.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = capacityBytes;
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

    // 持久映射:整个生命周期保持映射,避免每帧 Map/Unmap 开销。
    D3D12_RANGE noRead = {0, 0}; // CPU 不读
    void* mapped = nullptr;
    if (FAILED(self->m_resource->Map(0, &noRead, &mapped))) {
        return nullptr;
    }
    self->m_mapped = static_cast<uint8_t*>(mapped);
    return self;
}

void GpuBuffer::Write(const void* data, size_t bytes, size_t offsetBytes) {
    ME_ASSERT_MSG(m_mapped != nullptr, "GpuBuffer::Write: 仅动态缓冲可写");
    ME_ASSERT_MSG(offsetBytes + bytes <= m_size, "GpuBuffer::Write: 越界");
    std::memcpy(m_mapped + offsetBytes, data, bytes);
}

GpuBuffer::~GpuBuffer() {
    if (m_mapped != nullptr && m_resource) {
        m_resource->Unmap(0, nullptr);
        m_mapped = nullptr;
    }
}

} // namespace me::rhi
