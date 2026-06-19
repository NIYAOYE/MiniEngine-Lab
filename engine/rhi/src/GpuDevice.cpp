#include "me/rhi/GpuDevice.h"

#include "me/core/Log.h"

namespace me::rhi {

std::unique_ptr<GpuDevice> GpuDevice::Create(bool useWarp) {
    UINT factoryFlags = 0;
#if !defined(NDEBUG)
    // 开启 DX12 调试层,把 API 误用打到输出窗口。
    {
        ComPtr<ID3D12Debug> debug;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) {
            debug->EnableDebugLayer();
            factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }
    }
#endif

    auto self = std::make_unique<GpuDevice>();
    if (FAILED(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&self->m_factory)))) {
        ME_LOG_ERROR("CreateDXGIFactory2 失败");
        return nullptr;
    }

    // 选择适配器:WARP(软件)或第一个硬件适配器。
    ComPtr<IDXGIAdapter1> adapter;
    if (useWarp) {
        if (FAILED(self->m_factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter)))) {
            ME_LOG_ERROR("EnumWarpAdapter 失败");
            return nullptr;
        }
    } else {
        for (UINT i = 0; self->m_factory->EnumAdapters1(i, &adapter) !=
                         DXGI_ERROR_NOT_FOUND; ++i) {
            DXGI_ADAPTER_DESC1 desc{};
            adapter->GetDesc1(&desc);
            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue; // 跳过软件适配器
            if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0,
                                            __uuidof(ID3D12Device), nullptr))) {
                break;
            }
        }
    }
    if (adapter == nullptr) {
        ME_LOG_ERROR("未找到可用 DX12 适配器");
        return nullptr;
    }

    if (FAILED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0,
                                 IID_PPV_ARGS(&self->m_device)))) {
        ME_LOG_ERROR("D3D12CreateDevice 失败");
        return nullptr;
    }

    D3D12_COMMAND_QUEUE_DESC qd = {};
    qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    qd.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    if (FAILED(self->m_device->CreateCommandQueue(&qd, IID_PPV_ARGS(&self->m_queue)))) {
        ME_LOG_ERROR("CreateCommandQueue 失败");
        return nullptr;
    }
    return self;
}

} // namespace me::rhi
