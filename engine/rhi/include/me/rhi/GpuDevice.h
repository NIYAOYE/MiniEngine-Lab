#pragma once

#include <memory>
#include <d3d12.h>
#include <dxgi1_6.h>

#include "me/rhi/D3DCommon.h"

namespace me::rhi {

/**
 * @brief DX12 设备 + DIRECT 命令队列。裸 ID3D12* 仅经只读取用器外泄给同模块其它类。
 *
 * Create(useWarp=true) 选用 WARP 软件光栅器,使无独显/无头环境(测试)也能跑。
 */
class GpuDevice {
public:
    /** @brief 创建设备与命令队列;失败返回 nullptr(已记录原因)。 */
    static std::unique_ptr<GpuDevice> Create(bool useWarp);

    ID3D12Device* Device() const { return m_device.Get(); }
    ID3D12CommandQueue* Queue() const { return m_queue.Get(); }
    IDXGIFactory4* Factory() const { return m_factory.Get(); }

private:
    ComPtr<IDXGIFactory4> m_factory;
    ComPtr<ID3D12Device> m_device;
    ComPtr<ID3D12CommandQueue> m_queue;
};

} // namespace me::rhi
