#include <doctest/doctest.h>
#include <cstdint>
#include <vector>

#include "me/rhi/GpuDevice.h"
#include "me/rhi/Fence.h"
#include "me/rhi/DescriptorHeap.h"
#include "me/rhi/GpuTexture.h"
#include "me/rhi/Readback.h"

using namespace me::rhi;

TEST_CASE("2x2 纹理上传后逐像素回读一致") {
    auto device = GpuDevice::Create(true);
    REQUIRE(device != nullptr);
    auto fence = Fence::Create(device->Device());
    REQUIRE(fence != nullptr);
    auto srvHeap = DescriptorHeap::Create(
        device->Device(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 4, true);
    REQUIRE(srvHeap != nullptr);

    // 4 个不同颜色的 texel(RGBA8),行优先:左上、右上、左下、右下。
    const std::vector<uint8_t> pixels = {
        255, 0,   0,   255,   0,   255, 0,   255, // 红, 绿
        0,   0,   255, 255,   255, 255, 0,   255, // 蓝, 黄
    };
    auto srv = srvHeap->Allocate();
    auto tex = GpuTexture::Create(device->Device(), device->Queue(), *fence,
                                  2, 2, pixels.data(), srv);
    REQUIRE(tex != nullptr);

    auto out = ReadbackRgba8(device->Device(), device->Queue(), *fence,
                             tex->Resource(), 2, 2,
                             D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    REQUIRE(out.size() == pixels.size());
    for (size_t i = 0; i < pixels.size(); ++i) {
        CHECK(out[i] == pixels[i]);
    }
}
