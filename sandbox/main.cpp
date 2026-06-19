#include <memory>

#include "me/platform/Window.h"
#include "me/platform/Input.h"
#include "me/assets/ImageData.h"
#include "me/core/Log.h"
#include "me/core/Matrix4x4.h"
#include "me/core/Vector2.h"

#include "me/rhi/GpuDevice.h"
#include "me/rhi/SwapChain.h"
#include "me/rhi/Fence.h"
#include "me/rhi/CommandContext.h"
#include "me/rhi/DescriptorHeap.h"
#include "me/rhi/GpuTexture.h"
#include "me/rhi/SpriteTransform.h"
#include "me/render/SpriteRenderer.h"

#include <d3d12.h>

using namespace me;

namespace {
constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr float kSpritePixels = 64.0f; // 精灵边长(像素)
constexpr float kMoveSpeedPixels = 4.0f; // 每帧平移步长

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
} // namespace

int main() {
    platform::WindowDesc wd;
    wd.width = kWindowWidth;
    wd.height = kWindowHeight;
    wd.title = "MiniEngine M1 — Sprite";
    auto window = platform::Window::Create(wd);
    if (!window) { ME_LOG_ERROR("窗口创建失败"); return 1; }

    platform::InputState input;
    window->SetInput(&input);

    auto device = rhi::GpuDevice::Create(/*useWarp=*/false);
    if (!device) device = rhi::GpuDevice::Create(/*useWarp=*/true); // 回退软件
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

    auto image = assets::LoadImageRGBA8(std::string(ME_ASSET_DIR) +
                                        "/textures/test_sprite.png");
    if (!image) { ME_LOG_ERROR("加载贴图失败"); return 1; }

    auto srv = srvHeap->Allocate();
    auto texture = rhi::GpuTexture::Create(
        device->Device(), device->Queue(), *fence,
        static_cast<uint32_t>(image->width), static_cast<uint32_t>(image->height),
        image->pixels.data(), srv);
    auto renderer = render::SpriteRenderer::Create(*device);
    if (!texture || !renderer) { ME_LOG_ERROR("纹理/渲染器创建失败"); return 1; }

    // 世界空间正交投影:左下原点,Y 向上,单位=像素。
    const Matrix4x4 proj = Matrix4x4::Orthographic(
        0.0f, float(kWindowWidth), 0.0f, float(kWindowHeight), 0.0f, 1.0f);
    Vector2 spritePos{float(kWindowWidth) * 0.5f, float(kWindowHeight) * 0.5f};

    while (!window->ShouldClose()) {
        input.NewFrame();
        window->PumpMessages();
        if (input.WasPressed(platform::KeyCode::Escape)) break;
        if (input.IsDown(platform::KeyCode::A)) spritePos.x -= kMoveSpeedPixels;
        if (input.IsDown(platform::KeyCode::D)) spritePos.x += kMoveSpeedPixels;
        if (input.IsDown(platform::KeyCode::W)) spritePos.y += kMoveSpeedPixels;
        if (input.IsDown(platform::KeyCode::S)) spritePos.y -= kMoveSpeedPixels;

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
        const float clearColor[4] = {0.10f, 0.12f, 0.16f, 1.0f}; // 暗灰蓝清屏
        cmd->ClearRenderTargetView(rtv, clearColor, 0, nullptr);

        ID3D12DescriptorHeap* heaps[] = {srvHeap->Heap()};
        cmd->SetDescriptorHeaps(1, heaps);

        const Matrix4x4 model = rhi::MakeSpriteModelMatrix(
            spritePos, Vector2{kSpritePixels, kSpritePixels}, 0.0f);
        const Matrix4x4 mvp = model * proj; // 行向量:v * model * proj
        renderer->Draw(cmd, *texture, mvp);

        Transition(cmd, back, D3D12_RESOURCE_STATE_RENDER_TARGET,
                   D3D12_RESOURCE_STATE_PRESENT);
        ctx->End();
        ctx->Execute(device->Queue());
        swapChain->Present();
        fence->Flush(device->Queue()); // M1 简单同步:每帧等 GPU(M2 再做并行化)
        ctx->AdvanceFrame();
    }

    fence->Flush(device->Queue()); // 退出前确保 GPU 空闲再析构资源
    return 0;
}
