#include "me/rhi/Fence.h"

#include <windows.h>

namespace me::rhi {

std::unique_ptr<Fence> Fence::Create(ID3D12Device* device) {
    auto self = std::unique_ptr<Fence>(new Fence());
    if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE,
                                   IID_PPV_ARGS(&self->m_fence)))) {
        return nullptr;
    }
    self->m_event = ::CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (self->m_event == nullptr) {
        return nullptr;
    }
    return self;
}

Fence::~Fence() {
    if (m_event) ::CloseHandle(static_cast<HANDLE>(m_event));
}

uint64_t Fence::Signal(ID3D12CommandQueue* queue) {
    const uint64_t value = m_tracker.NextSignalValue();
    ME_HR_CHECK(queue->Signal(m_fence.Get(), value));
    return value;
}

void Fence::Wait(uint64_t value) {
    if (m_tracker.IsComplete(m_fence->GetCompletedValue(), value)) {
        return; // 已完成,无需挂起
    }
    ME_HR_CHECK(m_fence->SetEventOnCompletion(value, static_cast<HANDLE>(m_event)));
    ::WaitForSingleObject(static_cast<HANDLE>(m_event), INFINITE);
}

} // namespace me::rhi
