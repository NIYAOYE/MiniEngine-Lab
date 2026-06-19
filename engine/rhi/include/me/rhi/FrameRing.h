#pragma once

#include <cstdint>

namespace me::rhi {

/// 飞行中的帧数(双缓冲)。具名常量,贯穿命令分配器/围栏值数组尺寸。
constexpr uint32_t kFrameCount = 2;

/** @brief 在 [0, kFrameCount) 间循环的帧索引(选当前命令分配器/资源槽)。 */
struct FrameRing {
    uint32_t Current() const { return m_index; }
    /** @brief 推进到下一帧并返回新索引。 */
    uint32_t Advance() {
        m_index = (m_index + 1) % kFrameCount;
        return m_index;
    }

private:
    uint32_t m_index = 0;
};

} // namespace me::rhi
