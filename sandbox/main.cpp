#include <memory>
#include <vector>

#include "me/platform/Window.h"
#include "me/platform/Input.h"
#include "me/assets/ImageData.h"
#include "me/assets/TiledMapLoader.h"
#include "me/core/Log.h"
#include "me/core/Matrix4x4.h"
#include "me/core/Vector2.h"
#include "me/core/Vector4.h"
#include "me/core/Rect.h"

#include "me/rhi/GpuDevice.h"
#include "me/rhi/SwapChain.h"
#include "me/rhi/Fence.h"
#include "me/rhi/CommandContext.h"
#include "me/rhi/DescriptorHeap.h"
#include "me/rhi/GpuTexture.h"
#include "me/render/SpriteBatch.h"
#include "me/render/SpriteDesc.h"
#include "me/render/OrthographicCamera.h"
#include "me/render/Tileset.h"
#include "me/render/TileMapRenderer.h"

#include <d3d12.h>

using namespace me;

namespace {
constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr float kCameraSpeed = 6.0f;     // 每帧相机平移步长(像素)
constexpr float kZoomStep = 0.02f;       // 每帧缩放步长
constexpr float kMinZoom = 0.25f;
constexpr float kMaxZoom = 4.0f;

void Transition(ID3D12GraphicsCommandList* cmd, ID3D12Resource* res,
                D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to) {
    D3D12_RESOURCE_BARRIER b = {};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource = res;
    b.Transition.StateBefore = from;
    b.Transition.StateAfter = to;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmd->ResourceBarrier(1, &b);
}

float Clamp(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
} // namespace

int main() {
    platform::WindowDesc wd;
    wd.width = kWindowWidth;
    wd.height = kWindowHeight;
    wd.title = "MiniEngine M3 — TileMap";
    auto window = platform::Window::Create(wd);
    if (!window) { ME_LOG_ERROR("窗口创建失败"); return 1; }

    platform::InputState input;
    window->SetInput(&input);

    auto device = rhi::GpuDevice::Create(/*useWarp=*/false);
    if (!device) device = rhi::GpuDevice::Create(/*useWarp=*/true);
    if (!device) { ME_LOG_ERROR("DX12 设备创建失败"); return 1; }

    auto swapChain = rhi::SwapChain::Create(*device, window->NativeHandle(),
                                            kWindowWidth, kWindowHeight);
    auto ctx = rhi::CommandContext::Create(device->Device());
    auto fence = rhi::Fence::Create(device->Device());
    auto srvHeap = rhi::DescriptorHeap::Create(
        device->Device(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 8, true);
    if (!swapChain || !ctx || !fence || !srvHeap) {
        ME_LOG_ERROR("RHI 初始化失败"); return 1;
    }

    auto batch = render::SpriteBatch::Create(*device);
    if (!batch) { ME_LOG_ERROR("批渲染器创建失败"); return 1; }

    // 数据驱动:从 Tiled JSON 加载地图 + 其 tileset 贴图。
    auto mapData = assets::LoadTiledMap(std::string(ME_ASSET_DIR) + "/maps/demo.tmj");
    if (!mapData) { ME_LOG_ERROR("加载地图失败"); return 1; }
    auto tileImage = assets::LoadImageRGBA8(mapData->tileset.imagePath);
    if (!tileImage) { ME_LOG_ERROR("加载 tileset 贴图失败"); return 1; }
    auto tileSrv = srvHeap->Allocate();
    auto tileTex = rhi::GpuTexture::Create(
        device->Device(), device->Queue(), *fence,
        uint32_t(tileImage->width), uint32_t(tileImage->height),
        tileImage->pixels.data(), tileSrv);
    if (!tileTex) { ME_LOG_ERROR("创建 tileset 纹理失败"); return 1; }
    render::Tileset tileset(tileTex.get(), mapData->tileset);
    render::TileMapRenderer tileRenderer;

    // 相机居中于地图中心。
    render::OrthographicCamera camera;
    camera.SetViewportSize(float(kWindowWidth), float(kWindowHeight));
    camera.SetPosition(Vector2{mapData->mapCols * mapData->tileWidth * 0.5f,
                               mapData->mapRows * mapData->tileHeight * 0.5f});
    camera.SetZoom(1.0f);

    while (!window->ShouldClose()) {
        input.NewFrame();
        window->PumpMessages();
        if (input.WasPressed(platform::KeyCode::Escape)) break;

        // 输入驱动相机:WASD 平移、Q/E 缩放。
        Vector2 camPos = camera.Position();
        if (input.IsDown(platform::KeyCode::A)) camPos.x -= kCameraSpeed;
        if (input.IsDown(platform::KeyCode::D)) camPos.x += kCameraSpeed;
        if (input.IsDown(platform::KeyCode::W)) camPos.y += kCameraSpeed;
        if (input.IsDown(platform::KeyCode::S)) camPos.y -= kCameraSpeed;
        camera.SetPosition(camPos);
        float zoom = camera.Zoom();
        if (input.IsDown(platform::KeyCode::Q)) zoom -= kZoomStep;
        if (input.IsDown(platform::KeyCode::E)) zoom += kZoomStep;
        camera.SetZoom(Clamp(zoom, kMinZoom, kMaxZoom));

        auto* cmd = ctx->Begin();
        ID3D12Resource* back = swapChain->CurrentBackBuffer();
        D3D12_CPU_DESCRIPTOR_HANDLE rtv = swapChain->CurrentRtv();
        Transition(cmd, back, D3D12_RESOURCE_STATE_PRESENT,
                   D3D12_RESOURCE_STATE_RENDER_TARGET);

        D3D12_VIEWPORT vp = {0, 0, float(kWindowWidth), float(kWindowHeight), 0.0f, 1.0f};
        D3D12_RECT scissor = {0, 0, kWindowWidth, kWindowHeight};
        cmd->RSSetViewports(1, &vp);
        cmd->RSSetScissorRects(1, &scissor);
        cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
        const float clearColor[4] = {0.10f, 0.12f, 0.16f, 1.0f};
        cmd->ClearRenderTargetView(rtv, clearColor, 0, nullptr);

        ID3D12DescriptorHeap* heaps[] = {srvHeap->Heap()};
        cmd->SetDescriptorHeaps(1, heaps);

        // 渲染瓦片地图:调用方负责 Begin/End,TileMapRenderer 仅 Submit。
        batch->Begin(camera.ViewProj());
        tileRenderer.Render(*batch, camera, *mapData, tileset);
        batch->End(cmd);

        Transition(cmd, back, D3D12_RESOURCE_STATE_RENDER_TARGET,
                   D3D12_RESOURCE_STATE_PRESENT);
        ctx->End();
        ctx->Execute(device->Queue());
        swapChain->Present();
        fence->Flush(device->Queue()); // M3 仍每帧全同步(帧并行后续里程碑)
        ctx->AdvanceFrame();
    }

    fence->Flush(device->Queue());
    return 0;
}
