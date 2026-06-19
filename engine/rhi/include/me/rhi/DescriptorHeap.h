#pragma once

#include <cstdint>
#include <memory>
#include <d3d12.h>

#include "me/rhi/D3DCommon.h"
#include "me/rhi/DescriptorAllocatorLogic.h"

namespace me::rhi {

/** @brief 一个已分配描述符的 CPU/GPU 句柄与槽位序号。 */
struct Descriptor {
    D3D12_CPU_DESCRIPTOR_HANDLE cpu{};
    D3D12_GPU_DESCRIPTOR_HANDLE gpu{}; // 仅 shaderVisible 堆有效
    uint32_t index = 0;
};

/**
 * @brief 线性描述符堆:封装 ID3D12DescriptorHeap + 分配逻辑(DescriptorAllocatorLogic)。
 *
 * RTV 堆用非着色器可见;SRV(贴图)堆用着色器可见,以便着色阶段经描述符表采样。
 */
class DescriptorHeap {
public:
    static std::unique_ptr<DescriptorHeap> Create(ID3D12Device* device,
                                                  D3D12_DESCRIPTOR_HEAP_TYPE type,
                                                  uint32_t capacity,
                                                  bool shaderVisible);
    /** @brief 取下一个描述符槽;堆满则 ME_ASSERT(编程错误)。 */
    Descriptor Allocate();
    ID3D12DescriptorHeap* Heap() const { return m_heap.Get(); }

private:
    DescriptorHeap(uint32_t capacity, uint32_t increment)
        : m_logic(capacity, increment) {}

    ComPtr<ID3D12DescriptorHeap> m_heap;
    DescriptorAllocatorLogic m_logic;
    D3D12_CPU_DESCRIPTOR_HANDLE m_cpuStart{};
    D3D12_GPU_DESCRIPTOR_HANDLE m_gpuStart{};
    bool m_shaderVisible = false;
};

} // namespace me::rhi
