#include "me/rhi/SwapChain.h"

#include <windows.h>
#include "me/rhi/GpuDevice.h"

namespace me::rhi {

std::unique_ptr<SwapChain> SwapChain::Create(GpuDevice& device, void* hwnd,
                                             uint32_t width, uint32_t height) {
    auto self = std::unique_ptr<SwapChain>(new SwapChain());
    self->m_width = width;
    self->m_height = height;

    DXGI_SWAP_CHAIN_DESC1 desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = kFrameCount;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    ComPtr<IDXGISwapChain1> sc1;
    if (FAILED(device.Factory()->CreateSwapChainForHwnd(
            device.Queue(), static_cast<HWND>(hwnd), &desc, nullptr, nullptr, &sc1))) {
        return nullptr;
    }
    if (FAILED(sc1.As(&self->m_swapChain))) {
        return nullptr;
    }

    self->m_rtvHeap = DescriptorHeap::Create(
        device.Device(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, kFrameCount, false);
    if (self->m_rtvHeap == nullptr) {
        return nullptr;
    }

    for (uint32_t i = 0; i < kFrameCount; ++i) {
        if (FAILED(self->m_swapChain->GetBuffer(i, IID_PPV_ARGS(&self->m_backBuffers[i])))) {
            return nullptr;
        }
        Descriptor d = self->m_rtvHeap->Allocate();
        device.Device()->CreateRenderTargetView(
            self->m_backBuffers[i].Get(), nullptr, d.cpu);
        self->m_rtvs[i] = d.cpu;
    }
    return self;
}

} // namespace me::rhi
