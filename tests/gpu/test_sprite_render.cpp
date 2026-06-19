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
#include "me/core/Vector4.h"
#include "me/core/Rect.h"
#include "me/render/SpriteBatch.h"
#include "me/render/SpriteDesc.h"

using namespace me::rhi;
using me::render::SpriteBatch;
using me::render::SpriteDesc;

namespace {
constexpr uint32_t kRt = 8; // 8x8 离屏目标

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

TEST_CASE("SpriteBatch 单精灵:中心=贴图色,四角=清屏色") {
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

    const uint8_t red[4] = {255, 0, 0, 255};
    auto srv = srvHeap->Allocate();
    auto tex = GpuTexture::Create(device->Device(), device->Queue(), *fence,
                                  1, 1, red, srv);
    REQUIRE(tex != nullptr);

    auto batch = SpriteBatch::Create(*device);
    REQUIRE(batch != nullptr);

    // viewProj=8x8 像素正交;dstRect 覆盖中心 4x4(world [2,6]² → NDC [-0.5,0.5]²)。
    const me::Matrix4x4 vp = me::Matrix4x4::Orthographic(0, kRt, 0, kRt, 0, 1);
    SpriteDesc d;
    d.texture = tex.get();
    d.dstRect = me::Rect{2.0f, 2.0f, 4.0f, 4.0f};

    auto* cmd = ctx->Begin();
    D3D12_VIEWPORT vpRect = {0, 0, float(kRt), float(kRt), 0.0f, 1.0f};
    D3D12_RECT sc = {0, 0, LONG(kRt), LONG(kRt)};
    cmd->RSSetViewports(1, &vpRect);
    cmd->RSSetScissorRects(1, &sc);
    cmd->OMSetRenderTargets(1, &rtvDesc.cpu, FALSE, nullptr);
    const float blue[4] = {0.0f, 0.0f, 1.0f, 1.0f};
    cmd->ClearRenderTargetView(rtvDesc.cpu, blue, 0, nullptr);
    ID3D12DescriptorHeap* heaps[] = {srvHeap->Heap()};
    cmd->SetDescriptorHeaps(1, heaps);

    batch->Begin(vp);
    batch->Submit(d);
    batch->End(cmd);

    ctx->End();
    ctx->Execute(device->Queue());
    fence->Flush(device->Queue());

    auto px = ReadbackRgba8(device->Device(), device->Queue(), *fence,
                            rt.Get(), kRt, kRt, D3D12_RESOURCE_STATE_RENDER_TARGET);
    auto at = [&](uint32_t x, uint32_t y) { return &px[(y * kRt + x) * 4]; };

    CHECK(at(kRt/2, kRt/2)[0] > 200); // 中心红
    CHECK(at(kRt/2, kRt/2)[2] < 60);
    CHECK(at(0, 0)[2] > 200);         // 角蓝(清屏)
    CHECK(at(0, 0)[0] < 60);
    CHECK(batch->DrawCallCount() == 1);
}

TEST_CASE("SpriteBatch 色调:白贴图 × 绿色调 = 绿") {
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

    const uint8_t white[4] = {255, 255, 255, 255};
    auto srv = srvHeap->Allocate();
    auto tex = GpuTexture::Create(device->Device(), device->Queue(), *fence,
                                  1, 1, white, srv);
    REQUIRE(tex != nullptr);

    auto batch = SpriteBatch::Create(*device);
    REQUIRE(batch != nullptr);

    const me::Matrix4x4 vp = me::Matrix4x4::Orthographic(0, kRt, 0, kRt, 0, 1);
    SpriteDesc d;
    d.texture = tex.get();
    d.dstRect = me::Rect{0.0f, 0.0f, float(kRt), float(kRt)}; // 铺满
    d.color = me::Vector4{0.0f, 1.0f, 0.0f, 1.0f};            // 绿色调

    auto* cmd = ctx->Begin();
    D3D12_VIEWPORT vpRect = {0, 0, float(kRt), float(kRt), 0.0f, 1.0f};
    D3D12_RECT sc = {0, 0, LONG(kRt), LONG(kRt)};
    cmd->RSSetViewports(1, &vpRect);
    cmd->RSSetScissorRects(1, &sc);
    cmd->OMSetRenderTargets(1, &rtvDesc.cpu, FALSE, nullptr);
    const float black[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    cmd->ClearRenderTargetView(rtvDesc.cpu, black, 0, nullptr);
    ID3D12DescriptorHeap* heaps[] = {srvHeap->Heap()};
    cmd->SetDescriptorHeaps(1, heaps);

    batch->Begin(vp);
    batch->Submit(d);
    batch->End(cmd);

    ctx->End();
    ctx->Execute(device->Queue());
    fence->Flush(device->Queue());

    auto px = ReadbackRgba8(device->Device(), device->Queue(), *fence,
                            rt.Get(), kRt, kRt, D3D12_RESOURCE_STATE_RENDER_TARGET);
    auto at = [&](uint32_t x, uint32_t y) { return &px[(y * kRt + x) * 4]; };
    CHECK(at(kRt/2, kRt/2)[0] < 60);  // R 低
    CHECK(at(kRt/2, kRt/2)[1] > 200); // G 高
    CHECK(at(kRt/2, kRt/2)[2] < 60);  // B 低
}
