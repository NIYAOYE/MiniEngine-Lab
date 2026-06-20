#include <doctest/doctest.h>
#include <cstdint>
#include <vector>

#include "me/rhi/GpuDevice.h"
#include "me/rhi/Fence.h"
#include "me/rhi/DescriptorHeap.h"
#include "me/rhi/CommandContext.h"
#include "me/rhi/GpuTexture.h"
#include "me/rhi/Readback.h"
#include "me/render/SpriteBatch.h"
#include "me/render/Tileset.h"
#include "me/render/TileMapRenderer.h"
#include "me/render/OrthographicCamera.h"
#include "me/assets/TileMapData.h"

using namespace me::rhi;
using me::render::SpriteBatch;
using me::render::Tileset;
using me::render::TileMapRenderer;
using me::render::OrthographicCamera;

namespace {
constexpr uint32_t kRt = 32; // 渲染目标边长(像素)= 2x2 瓦片 x 16px

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

TEST_CASE("TileMapRenderer:2x2 单层地图——四角色块正确 + 全合 1 drawcall") {
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

    // 2x2 像素图集当 tileset:左上红、右上绿、左下蓝、右下白(行主序 row0=上)。
    // 每个 texel 即一个 1x1 "瓦片"(tileW=tileH=1,columns=2,image 2x2)。
    const uint8_t atlas[16] = {
        255, 0, 0, 255,   0, 255, 0, 255,
        0, 0, 255, 255,   255, 255, 255, 255
    };
    auto srv = srvHeap->Allocate();
    auto tex = GpuTexture::Create(device->Device(), device->Queue(), *fence,
                                  2, 2, atlas, srv);
    REQUIRE(tex != nullptr);
    auto batch = SpriteBatch::Create(*device);
    REQUIRE(batch != nullptr);

    // 地图:2 列 x 2 行,瓦片 16px。gid 选取使四角颜色可辨。
    // localId: 0=红(左上 texel), 1=绿(右上), 2=蓝(左下), 3=白(右下)。firstgid=1。
    me::assets::TileMapData map;
    map.mapCols = 2; map.mapRows = 2; map.tileWidth = 16; map.tileHeight = 16;
    map.tileset.tileWidth = 1; map.tileset.tileHeight = 1;
    map.tileset.columns = 2; map.tileset.imageWidth = 2; map.tileset.imageHeight = 2;
    map.tileset.firstGid = 1;
    me::assets::TileLayer layer;
    layer.name = "ground";
    // 行主序 row0 在顶:顶行[红=gid1, 绿=gid2],底行[蓝=gid3, 白=gid4]。
    layer.gids = {1, 2, 3, 4};
    map.layers.push_back(layer);

    Tileset tileset(tex.get(), map.tileset);
    TileMapRenderer renderer;

    // 相机覆盖整图(32x32 世界,居中 16,16)。
    OrthographicCamera camera;
    camera.SetViewportSize(float(kRt), float(kRt));
    camera.SetPosition(me::Vector2{16.0f, 16.0f});
    camera.SetZoom(1.0f);

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

    batch->Begin(camera.ViewProj());
    renderer.Render(*batch, camera, map, tileset);
    batch->End(cmd);

    ctx->End();
    ctx->Execute(device->Queue());
    fence->Flush(device->Queue());

    CHECK(batch->DrawCallCount() == 1); // 单 tileset → 全部瓦片合 1 次

    auto px = ReadbackRgba8(device->Device(), device->Queue(), *fence,
                            rt.Get(), kRt, kRt, D3D12_RESOURCE_STATE_RENDER_TARGET);
    auto at = [&](uint32_t x, uint32_t y) { return &px[(y * kRt + x) * 4]; };
    // 屏幕 Y 向下;世界 Y 向上 → Tiled 顶行(红/绿)在屏幕上半(y 小)。
    // 左上瓦片(gid1=红):屏幕 (8, 8)
    CHECK(at(8, 8)[0] > 200);  CHECK(at(8, 8)[1] < 60);  CHECK(at(8, 8)[2] < 60);
    // 右上瓦片(gid2=绿):屏幕 (24, 8)
    CHECK(at(24, 8)[1] > 200); CHECK(at(24, 8)[0] < 60);
    // 左下瓦片(gid3=蓝):屏幕 (8, 24)
    CHECK(at(8, 24)[2] > 200); CHECK(at(8, 24)[0] < 60);
    // 右下瓦片(gid4=白):屏幕 (24, 24)
    CHECK(at(24, 24)[0] > 200); CHECK(at(24, 24)[1] > 200); CHECK(at(24, 24)[2] > 200);
}
