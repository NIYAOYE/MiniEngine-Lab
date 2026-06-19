#pragma once

#include <array>
#include <memory>
#include <d3d12.h>

#include "me/rhi/D3DCommon.h"
#include "me/rhi/FrameRing.h"

namespace me::rhi {

/**
 * @brief 每帧命令录制:kFrameCount 个命令分配器 + 一个图形命令列表。
 *
 * Begin() 重置当前帧分配器与列表并返回录制态列表;End() 关闭;Execute() 提交。
 * 双缓冲下,只要等待对应帧的围栏完成即可安全重置该帧分配器。
 */
class CommandContext {
public:
    static std::unique_ptr<CommandContext> Create(ID3D12Device* device);

    ID3D12GraphicsCommandList* Begin();
    void End();
    ID3D12GraphicsCommandList* List() const { return m_list.Get(); }
    void Execute(ID3D12CommandQueue* queue);
    void AdvanceFrame() { m_ring.Advance(); }
    uint32_t FrameIndex() const { return m_ring.Current(); }

private:
    CommandContext() = default;
    std::array<ComPtr<ID3D12CommandAllocator>, kFrameCount> m_allocators;
    ComPtr<ID3D12GraphicsCommandList> m_list;
    FrameRing m_ring;
};

} // namespace me::rhi
