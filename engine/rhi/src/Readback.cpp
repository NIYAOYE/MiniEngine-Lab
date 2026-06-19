#include "me/rhi/Readback.h"

#include <cstring>
#include "me/rhi/D3DCommon.h"
#include "me/rhi/Fence.h"

namespace me::rhi {

namespace {
constexpr uint32_t kBytesPerPixel = 4;
} // namespace

std::vector<uint8_t> ReadbackRgba8(ID3D12Device* device,
                                   ID3D12CommandQueue* queue,
                                   Fence& fence,
                                   ID3D12Resource* tex,
                                   uint32_t width, uint32_t height,
                                   D3D12_RESOURCE_STATES beforeState) {
    D3D12_RESOURCE_DESC texDesc = tex->GetDesc();
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
    UINT numRows = 0;
    UINT64 rowSizeBytes = 0, totalBytes = 0;
    device->GetCopyableFootprints(&texDesc, 0, 1, 0, &footprint, &numRows,
                                  &rowSizeBytes, &totalBytes);

    // 回读堆缓冲。
    D3D12_HEAP_PROPERTIES readbackHeap = {};
    readbackHeap.Type = D3D12_HEAP_TYPE_READBACK;
    D3D12_RESOURCE_DESC bufDesc = {};
    bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufDesc.Width = totalBytes;
    bufDesc.Height = 1;
    bufDesc.DepthOrArraySize = 1;
    bufDesc.MipLevels = 1;
    bufDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufDesc.SampleDesc.Count = 1;
    bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    ComPtr<ID3D12Resource> readback;
    ME_HR_CHECK(device->CreateCommittedResource(
        &readbackHeap, D3D12_HEAP_FLAG_NONE, &bufDesc,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readback)));

    ComPtr<ID3D12CommandAllocator> alloc;
    ComPtr<ID3D12GraphicsCommandList> list;
    ME_HR_CHECK(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                               IID_PPV_ARGS(&alloc)));
    ME_HR_CHECK(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                          alloc.Get(), nullptr, IID_PPV_ARGS(&list)));

    auto transition = [&](D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to) {
        D3D12_RESOURCE_BARRIER b = {};
        b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b.Transition.pResource = tex;
        b.Transition.StateBefore = from;
        b.Transition.StateAfter = to;
        b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        list->ResourceBarrier(1, &b);
    };

    transition(beforeState, D3D12_RESOURCE_STATE_COPY_SOURCE);
    D3D12_TEXTURE_COPY_LOCATION dst = {};
    dst.pResource = readback.Get();
    dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    dst.PlacedFootprint = footprint;
    D3D12_TEXTURE_COPY_LOCATION src = {};
    src.pResource = tex;
    src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    src.SubresourceIndex = 0;
    list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    transition(D3D12_RESOURCE_STATE_COPY_SOURCE, beforeState);

    ME_HR_CHECK(list->Close());
    ID3D12CommandList* lists[] = {list.Get()};
    queue->ExecuteCommandLists(1, lists);
    fence.Flush(queue);

    // 去掉行填充,拷成紧凑 RGBA8。
    std::vector<uint8_t> out(static_cast<size_t>(width) * height * kBytesPerPixel);
    uint8_t* mapped = nullptr;
    D3D12_RANGE readRange = {0, static_cast<SIZE_T>(totalBytes)};
    ME_HR_CHECK(readback->Map(0, &readRange, reinterpret_cast<void**>(&mapped)));
    const uint32_t dstRowBytes = width * kBytesPerPixel;
    for (uint32_t row = 0; row < height; ++row) {
        std::memcpy(out.data() + row * dstRowBytes,
                    mapped + footprint.Offset + row * footprint.Footprint.RowPitch,
                    dstRowBytes);
    }
    D3D12_RANGE noWrite = {0, 0};
    readback->Unmap(0, &noWrite);
    return out;
}

} // namespace me::rhi
