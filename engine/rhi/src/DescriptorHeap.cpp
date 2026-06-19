#include "me/rhi/DescriptorHeap.h"

#include <cstdlib>

#include "me/core/Assert.h"
#include "me/core/Log.h"

namespace me::rhi {

std::unique_ptr<DescriptorHeap> DescriptorHeap::Create(
    ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type,
    uint32_t capacity, bool shaderVisible) {

    const uint32_t increment = device->GetDescriptorHandleIncrementSize(type);
    auto self = std::unique_ptr<DescriptorHeap>(new DescriptorHeap(capacity, increment));
    self->m_shaderVisible = shaderVisible;

    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = type;
    desc.NumDescriptors = capacity;
    desc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
                               : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    if (FAILED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&self->m_heap)))) {
        return nullptr;
    }
    self->m_cpuStart = self->m_heap->GetCPUDescriptorHandleForHeapStart();
    if (shaderVisible) {
        self->m_gpuStart = self->m_heap->GetGPUDescriptorHandleForHeapStart();
    }
    return self;
}

Descriptor DescriptorHeap::Allocate() {
    auto slot = m_logic.Allocate();
    ME_ASSERT_MSG(slot.has_value(), "描述符堆已满");
    if (!slot.has_value()) {
        // Release(NDEBUG)下 ME_ASSERT 被编译为空操作:此处兜底,避免解引用空
        // optional 的未定义行为。描述符堆容量在初始化期确定,堆满属编程错误,
        // 与 ME_HR_CHECK 一致地直接中止(而非返回错误句柄静默继续)。
        ME_LOG_ERROR("DescriptorHeap 已满(容量不足,属编程错误)");
        std::abort();
    }
    const uint64_t offset = m_logic.CpuOffsetBytes(*slot);

    Descriptor d{};
    d.index = *slot;
    d.cpu.ptr = m_cpuStart.ptr + offset;
    if (m_shaderVisible) {
        d.gpu.ptr = m_gpuStart.ptr + offset;
    }
    return d;
}

} // namespace me::rhi
