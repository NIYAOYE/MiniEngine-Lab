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
#include "me/core/Vector4.h"
#include "me/core/Rect.h"
#include "me/render/SpriteBatch.h"
#include "me/render/SpriteDesc.h"

using namespace me::rhi;
using me::render::SpriteBatch;
using me::render::SpriteDesc;

namespace {
constexpr uint32_t kRt = 8;

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

// 1x1 纯色纹理 helper(各自占一个 SRV)。
std::unique_ptr<GpuTexture> MakeSolidTexture(GpuDevice& device, Fence& fence,
                                             DescriptorHeap& srvHeap,
                                             uint8_t r, uint8_t g, uint8_t b) {
    const uint8_t px[4] = {r, g, b, 255};
    auto srv = srvHeap.Allocate();
    return GpuTexture::Create(device.Device(), device.Queue(), fence, 1, 1, px, srv);
}
} // namespace

TEST_CASE("SpriteBatch 分组:同纹理合 1 次,两纹理 2 次,与提交顺序无关") {
    auto device = GpuDevice::Create(true);
    REQUIRE(device != nullptr);
    auto fence = Fence::Create(device->Device());
    auto ctx = CommandContext::Create(device->Device());
    auto rtvHeap = DescriptorHeap::Create(device->Device(),
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1, false);
    auto srvHeap = DescriptorHeap::Create(device->Device(),
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 4, true);
    REQUIRE(fence); REQUIRE(ctx); REQUIRE(rtvHeap); REQUIRE(srvHeap);
    auto rtvDesc = rtvHeap->Allocate();
    auto rt = MakeRenderTarget(device->Device(), rtvDesc.cpu);

    auto texA = MakeSolidTexture(*device, *fence, *srvHeap, 255, 0, 0);
    auto texB = MakeSolidTexture(*device, *fence, *srvHeap, 0, 255, 0);
    REQUIRE(texA != nullptr); REQUIRE(texB != nullptr);
    auto batch = SpriteBatch::Create(*device);
    REQUIRE(batch != nullptr);

    const me::Matrix4x4 vp = me::Matrix4x4::Orthographic(0, kRt, 0, kRt, 0, 1);
    auto sprite = [](const GpuTexture* t, float x) {
        SpriteDesc d; d.texture = t; d.dstRect = me::Rect{x, 0.0f, 4.0f, 8.0f}; return d;
    };

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

    // 交错提交 A,B,A:稳定排序后 [A,A,B] → 2 次 drawcall。
    batch->Begin(vp);
    batch->Submit(sprite(texA.get(), 0.0f));
    batch->Submit(sprite(texB.get(), 4.0f));
    batch->Submit(sprite(texA.get(), 0.0f));
    batch->End(cmd);

    ctx->End();
    ctx->Execute(device->Queue());
    fence->Flush(device->Queue());

    CHECK(batch->DrawCallCount() == 2);

    auto px = ReadbackRgba8(device->Device(), device->Queue(), *fence,
                            rt.Get(), kRt, kRt, D3D12_RESOURCE_STATE_RENDER_TARGET);
    auto at = [&](uint32_t x, uint32_t y) { return &px[(y * kRt + x) * 4]; };
    CHECK(at(2, 4)[0] > 200); // 左半红(texA)
    CHECK(at(6, 4)[1] > 200); // 右半绿(texB)
}

TEST_CASE("SpriteBatch srcRect:从 2x2 纹理取左上角 texel") {
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

    // 2x2:左上红、右上绿、左下蓝、右下白(行主序,row0 = 上)。
    const uint8_t pixels[16] = {
        255, 0, 0, 255,   0, 255, 0, 255,   // 上行:红 绿
        0, 0, 255, 255,   255, 255, 255, 255 // 下行:蓝 白
    };
    auto srv = srvHeap->Allocate();
    auto tex = GpuTexture::Create(device->Device(), device->Queue(), *fence,
                                  2, 2, pixels, srv);
    REQUIRE(tex != nullptr);
    auto batch = SpriteBatch::Create(*device);
    REQUIRE(batch != nullptr);

    const me::Matrix4x4 vp = me::Matrix4x4::Orthographic(0, kRt, 0, kRt, 0, 1);
    SpriteDesc d;
    d.texture = tex.get();
    d.dstRect = me::Rect{0.0f, 0.0f, float(kRt), float(kRt)};
    d.srcRect = me::Rect{0.0f, 0.0f, 0.5f, 0.5f}; // 左上 texel(UV 原点左上,V 向下)

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
    // 整个 dst 只采样左上 texel → 处处为红。
    CHECK(at(kRt/2, kRt/2)[0] > 200);
    CHECK(at(kRt/2, kRt/2)[1] < 60);
    CHECK(at(kRt/2, kRt/2)[2] < 60);
}

TEST_CASE("SpriteBatch 容量增长:提交超过初始容量仍单纹理合 1 次") {
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

    auto tex = MakeSolidTexture(*device, *fence, *srvHeap, 255, 0, 0);
    REQUIRE(tex != nullptr);
    auto batch = SpriteBatch::Create(*device);
    REQUIRE(batch != nullptr);

    const me::Matrix4x4 vp = me::Matrix4x4::Orthographic(0, kRt, 0, kRt, 0, 1);
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
    // 超过初始容量 → End 内 EnsureCapacity 增长缓冲,不丢精灵。
    const uint32_t n = me::render::kInitialSpriteCapacity + 1;
    SpriteDesc d; d.texture = tex.get(); d.dstRect = me::Rect{2.0f, 2.0f, 4.0f, 4.0f};
    for (uint32_t i = 0; i < n; ++i) batch->Submit(d);
    batch->End(cmd);

    ctx->End();
    ctx->Execute(device->Queue());
    fence->Flush(device->Queue());

    CHECK(batch->DrawCallCount() == 1); // 同纹理 → 仍 1 次
    auto px = ReadbackRgba8(device->Device(), device->Queue(), *fence,
                            rt.Get(), kRt, kRt, D3D12_RESOURCE_STATE_RENDER_TARGET);
    auto at = [&](uint32_t x, uint32_t y) { return &px[(y * kRt + x) * 4]; };
    CHECK(at(kRt/2, kRt/2)[0] > 200); // 中心红
}
