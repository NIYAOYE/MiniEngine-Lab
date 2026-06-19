#pragma once

#include <cstdint>

namespace me::rhi {

/**
 * @brief GPU 围栏的纯逻辑部分:产生单调递增的信号值并判定是否已完成。
 *
 * 不含任何 DX12 依赖;真正的 ID3D12Fence 在 Fence 类里持有,调用本结构取值。
 * 这是 DX12 同步模型的核心:CPU 给队列排一个递增的 fence 值,GPU 执行到时回写,
 * CPU 比较 completedValue >= target 即知该批命令已结束。
 */
struct FenceTracker {
    /** @brief 取下一个待排队的信号值(从 1 开始,0 表示"尚未提交")。 */
    uint64_t NextSignalValue() { return m_next++; }

    /** @brief GPU 已完成值 completed 是否达到/超过目标 target。 */
    bool IsComplete(uint64_t completed, uint64_t target) const {
        return completed >= target;
    }

private:
    uint64_t m_next = 1;
};

} // namespace me::rhi
