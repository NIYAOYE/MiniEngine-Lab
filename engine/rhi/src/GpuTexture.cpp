#include "me/rhi/GpuTexture.h"

#include <cstring>
#include "me/rhi/Fence.h"

namespace me::rhi {

namespace {
constexpr uint32_t kBytesPerPixel = 4; // RGBA8
} // namespace

std::unique_ptr<GpuTexture> GpuTexture::Create(ID3D12Device* device,
                                               ID3D12CommandQueue* queue,
                                               Fence& fence,
                                               uint32_t width, uint32_t height,
                                               const uint8_t* rgba8,
                                               const Descriptor& srv) {
    auto self = std::unique_ptr<GpuTexture>(new GpuTexture());
    self->m_width = width;
    self->m_height = height;

    // 1) 默认堆纹理(COPY_DEST 起始)。
    D3D12_HEAP_PROPERTIES defaultHeap = {};
    defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = kSpriteTextureFormat;
    texDesc.SampleDesc.Count = 1;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

    if (FAILED(device->CreateCommittedResource(
            &defaultHeap, D3D12_HEAP_FLAG_NONE, &texDesc,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
            IID_PPV_ARGS(&self->m_resource)))) {
        return nullptr;
    }

    // 2) 计算可拷贝足迹,建上传缓冲。
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
    UINT numRows = 0;
    UINT64 rowSizeBytes = 0, totalBytes = 0;
    device->GetCopyableFootprints(&texDesc, 0, 1, 0, &footprint, &numRows,
                                  &rowSizeBytes, &totalBytes);

    D3D12_HEAP_PROPERTIES uploadHeap = {};
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC bufDesc = {};
    bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufDesc.Width = totalBytes;
    bufDesc.Height = 1;
    bufDesc.DepthOrArraySize = 1;
    bufDesc.MipLevels = 1;
    bufDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufDesc.SampleDesc.Count = 1;
    bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    ComPtr<ID3D12Resource> upload;
    if (FAILED(device->CreateCommittedResource(
            &uploadHeap, D3D12_HEAP_FLAG_NONE, &bufDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&upload)))) {
        return nullptr;
    }

    // 3) 按行(对齐到 footprint.Footprint.RowPitch)拷入像素。
    uint8_t* mapped = nullptr;
    D3D12_RANGE noRead = {0, 0};
    if (FAILED(upload->Map(0, &noRead, reinterpret_cast<void**>(&mapped)))) {
        return nullptr;
    }
    const uint32_t srcRowBytes = width * kBytesPerPixel;
    for (uint32_t row = 0; row < height; ++row) {
        std::memcpy(mapped + footprint.Offset + row * footprint.Footprint.RowPitch,
                    rgba8 + row * srcRowBytes, srcRowBytes);
    }
    upload->Unmap(0, nullptr);

    // 4) 录制拷贝 + 状态转换(自建一次性命令列表)。
    ComPtr<ID3D12CommandAllocator> alloc;
    ComPtr<ID3D12GraphicsCommandList> list;
    if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                              IID_PPV_ARGS(&alloc))) ||
        FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                         alloc.Get(), nullptr, IID_PPV_ARGS(&list)))) {
        return nullptr;
    }

    D3D12_TEXTURE_COPY_LOCATION dst = {};
    dst.pResource = self->m_resource.Get();
    dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst.SubresourceIndex = 0;
    D3D12_TEXTURE_COPY_LOCATION src = {};
    src.pResource = upload.Get();
    src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    src.PlacedFootprint = footprint;
    list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = self->m_resource.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    list->ResourceBarrier(1, &barrier);

    ME_HR_CHECK(list->Close());
    ID3D12CommandList* lists[] = {list.Get()};
    queue->ExecuteCommandLists(1, lists);
    fence.Flush(queue); // 同步等待上传完成,之后 upload/alloc 可安全释放

    // 5) 建 SRV。
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = kSpriteTextureFormat;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = 1;
    device->CreateShaderResourceView(self->m_resource.Get(), &srvDesc, srv.cpu);
    self->m_srvGpu = srv.gpu;
    return self;
}

} // namespace me::rhi
