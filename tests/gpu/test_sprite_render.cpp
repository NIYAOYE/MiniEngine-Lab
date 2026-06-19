#include <doctest/doctest.h>
#include <cstdint>
#include <vector>

#include "me/rhi/GpuDevice.h"
#include "me/rhi/Fence.h"
#include "me/rhi/DescriptorHeap.h"
#include "me/rhi/CommandContext.h"
#include "me/rhi/GpuTexture.h"
#include "me/rhi/Readback.h"
#include "me/core/Matrix4x4.h"
#include "me/core/Vector2.h"
#include "me/render/SpriteRenderer.h"

using namespace me::rhi;
using me::render::SpriteRenderer;

namespace {
constexpr uint32_t kRt = 8; // 8x8 离屏目标

// 创建一个 8x8、RENDER_TARGET 起始状态的离屏纹理 + RTV。
ComPtr<ID3D12Resource> MakeRenderTarget(ID3D12Device* device,
                                        D3D12_CPU_DESCRIPTOR_HANDLE rtv) {
    D3D12_HEAP_PROPERTIES heap = {};
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = kRt; desc.Height = kRt;
    desc.DepthOrArraySize = 1; desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    D3D12_CLEAR_VALUE clear = {};
    clear.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    ComPtr<ID3D12Resource> rt;
    device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
        D3D12_RESOURCE_STATE_RENDER_TARGET, &clear, IID_PPV_ARGS(&rt));
    device->CreateRenderTargetView(rt.Get(), nullptr, rtv);
    return rt;
}
} // namespace

TEST_CASE("带纹理精灵:中心=贴图色,四角=清屏色") {
    auto device = GpuDevice::Create(true);
    REQUIRE(device != nullptr);
    auto fence = Fence::Create(device->Device());
    auto ctx = CommandContext::Create(device->Device());
    auto rtvHeap = DescriptorHeap::Create(device->Device(),
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1, false);
    auto srvHeap = DescriptorHeap::Create(device->Device(),
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1, true);
    REQUIRE(fence); REQUIRE(ctx); REQUIRE(rtvHeap); REQUIRE(srvHeap);

    auto rtvDesc = rtvHeap->Allocate();
    auto rt = MakeRenderTarget(device->Device(), rtvDesc.cpu);

    // 1x1 纯红贴图 → 任意采样都得红,回读判定稳定。
    const uint8_t red[4] = {255, 0, 0, 255};
    auto srv = srvHeap->Allocate();
    auto tex = GpuTexture::Create(device->Device(), device->Queue(), *fence,
                                  1, 1, red, srv);
    REQUIRE(tex != nullptr);

    auto renderer = SpriteRenderer::Create(*device);
    REQUIRE(renderer != nullptr);

    // 投影=单位,模型=缩放 0.5:四边形覆盖 NDC 中央 1/4,四角留清屏色。
    me::Matrix4x4 mvp = me::Matrix4x4::Scale(me::Vector2{0.5f, 0.5f});

    auto* cmd = ctx->Begin();
    D3D12_VIEWPORT vp = {0, 0, float(kRt), float(kRt), 0.0f, 1.0f};
    D3D12_RECT sc = {0, 0, LONG(kRt), LONG(kRt)};
    cmd->RSSetViewports(1, &vp);
    cmd->RSSetScissorRects(1, &sc);
    cmd->OMSetRenderTargets(1, &rtvDesc.cpu, FALSE, nullptr);
    const float blue[4] = {0.0f, 0.0f, 1.0f, 1.0f};
    cmd->ClearRenderTargetView(rtvDesc.cpu, blue, 0, nullptr);
    ID3D12DescriptorHeap* heaps[] = {srvHeap->Heap()};
    cmd->SetDescriptorHeaps(1, heaps);
    renderer->Draw(cmd, *tex, mvp);
    ctx->End();
    ctx->Execute(device->Queue());
    fence->Flush(device->Queue());

    auto px = ReadbackRgba8(device->Device(), device->Queue(), *fence,
                            rt.Get(), kRt, kRt, D3D12_RESOURCE_STATE_RENDER_TARGET);
    auto at = [&](uint32_t x, uint32_t y) { return &px[(y * kRt + x) * 4]; };

    // 中心像素 ≈ 红
    CHECK(at(kRt/2, kRt/2)[0] > 200);
    CHECK(at(kRt/2, kRt/2)[2] < 60);
    // 角像素 ≈ 蓝(清屏色)
    CHECK(at(0, 0)[2] > 200);
    CHECK(at(0, 0)[0] < 60);
}
