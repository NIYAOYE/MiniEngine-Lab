#pragma once

#include <cstdint>
#include <memory>
#include <d3d12.h>

#include "me/rhi/D3DCommon.h"
#include "me/rhi/FenceTracker.h"

namespace me::rhi {

/**
 * @brief CPU↔GPU 同步围栏:把递增信号值排进队列,并能阻塞等待其完成。
 *
 * 复用 FenceTracker 产生单调信号值;Win32 事件用于 Wait 时挂起 CPU 线程。
 */
class Fence {
public:
    static std::unique_ptr<Fence> Create(ID3D12Device* device);
    ~Fence();
    Fence(const Fence&) = delete;
    Fence& operator=(const Fence&) = delete;

    /** @brief 在队列尾排入一个新信号值并返回它。 */
    uint64_t Signal(ID3D12CommandQueue* queue);
    /** @brief 阻塞 CPU 直到 GPU 完成给定信号值。 */
    void Wait(uint64_t value);
    /** @brief Signal + Wait:清空队列(M1 简单同步,每帧用)。 */
    void Flush(ID3D12CommandQueue* queue) { Wait(Signal(queue)); }

    uint64_t CompletedValue() const { return m_fence->GetCompletedValue(); }

private:
    Fence() = default;
    ComPtr<ID3D12Fence> m_fence;
    void* m_event = nullptr; // HANDLE
    FenceTracker m_tracker;
};

} // namespace me::rhi
