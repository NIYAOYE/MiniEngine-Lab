#pragma once

#include <cstdint>
#include <optional>

namespace me::rhi {

/**
 * @brief 描述符堆的线性分配逻辑(纯算术,不持有 ID3D12DescriptorHeap)。
 *
 * 真正的 DescriptorHeap 把堆起始 CPU/GPU 句柄 + 本逻辑组合:Allocate() 给出
 * 槽位序号,再用 CpuOffsetBytes(index) 加到堆起始句柄上得到具体描述符位置。
 * increment 来自 device->GetDescriptorHandleIncrementSize(heapType),运行时查询。
 */
struct DescriptorAllocatorLogic {
    DescriptorAllocatorLogic(uint32_t capacity, uint32_t increment)
        : m_capacity(capacity), m_increment(increment) {}

    /** @brief 分配一个槽位序号;堆已满返回 std::nullopt。 */
    std::optional<uint32_t> Allocate() {
        if (m_count >= m_capacity) return std::nullopt;
        return m_count++;
    }

    /** @brief 槽位序号 → 相对堆起始的字节偏移。 */
    uint64_t CpuOffsetBytes(uint32_t index) const {
        return static_cast<uint64_t>(index) * m_increment;
    }

    uint32_t Count() const { return m_count; }
    uint32_t Capacity() const { return m_capacity; }

private:
    uint32_t m_capacity;
    uint32_t m_increment;
    uint32_t m_count = 0;
};

} // namespace me::rhi
