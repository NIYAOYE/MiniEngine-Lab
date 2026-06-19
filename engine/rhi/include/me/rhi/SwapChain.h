#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <d3d12.h>
#include <dxgi1_6.h>

#include "me/rhi/D3DCommon.h"
#include "me/rhi/FrameRing.h"
#include "me/rhi/DescriptorHeap.h"

namespace me::rhi {

class GpuDevice;

/**
 * @brief DXGI 翻转模型交换链 + 每后台缓冲一个 RTV。
 *
 * 翻转模型(FLIP_DISCARD)是 DX12 的现代呈现路径。kFrameCount 个后台缓冲与
 * CommandContext 的帧分配器一一对应。
 */
class SwapChain {
public:
    static std::unique_ptr<SwapChain> Create(GpuDevice& device, void* hwnd,
                                             uint32_t width, uint32_t height);

    uint32_t BackBufferIndex() const { return m_swapChain->GetCurrentBackBufferIndex(); }
    ID3D12Resource* CurrentBackBuffer() const {
        return m_backBuffers[BackBufferIndex()].Get();
    }
    D3D12_CPU_DESCRIPTOR_HANDLE CurrentRtv() const { return m_rtvs[BackBufferIndex()]; }
    void Present() { m_swapChain->Present(1, 0); } // 垂直同步

    uint32_t Width() const { return m_width; }
    uint32_t Height() const { return m_height; }

private:
    SwapChain() = default;
    ComPtr<IDXGISwapChain3> m_swapChain;
    std::unique_ptr<DescriptorHeap> m_rtvHeap;
    std::array<ComPtr<ID3D12Resource>, kFrameCount> m_backBuffers;
    std::array<D3D12_CPU_DESCRIPTOR_HANDLE, kFrameCount> m_rtvs{};
    uint32_t m_width = 0;
    uint32_t m_height = 0;
};

} // namespace me::rhi
