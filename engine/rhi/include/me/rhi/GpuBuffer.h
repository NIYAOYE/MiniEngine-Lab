#pragma once

#include <cstddef>
#include <memory>
#include <d3d12.h>

#include "me/rhi/D3DCommon.h"

namespace me::rhi {

/**
 * @brief GPU 上传堆缓冲:CPU 可写、GPU 可读。
 *
 * 提供两种创建模式:
 *  - CreateUpload:一次性写入后解映射(M1 静态顶点/索引缓冲)。
 *  - CreateDynamic:持久映射,供每帧通过 Write() 更新(M2 SpriteBatch 用)。
 *
 * 性能审查点:上传堆位于系统内存,GPU 每次读取需过 PCIe。生产应改用默认堆 +
 * 一次性拷贝。M1/M2 以可读性优先,先这样。
 */
class GpuBuffer {
public:
    /// @brief 上传堆缓冲:创建即写入 data 并解映射(M1 顶点/索引用)。
    static std::unique_ptr<GpuBuffer> CreateUpload(ID3D12Device* device,
                                                   const void* data,
                                                   size_t sizeBytes);

    /// @brief 动态上传堆缓冲:容量 capacityBytes,构造后保持持久映射,供每帧 Write。
    static std::unique_ptr<GpuBuffer> CreateDynamic(ID3D12Device* device,
                                                    size_t capacityBytes);

    ~GpuBuffer();

    /// @brief 把 bytes 字节从 data 拷入映射区 offsetBytes 处(仅动态缓冲有效)。
    void Write(const void* data, size_t bytes, size_t offsetBytes = 0);

    D3D12_GPU_VIRTUAL_ADDRESS Gpu() const { return m_resource->GetGPUVirtualAddress(); }
    size_t Size() const { return m_size; }
    ID3D12Resource* Resource() const { return m_resource.Get(); }
    /// @brief 持久映射指针;CreateUpload 创建的对象返回 nullptr。
    const uint8_t* Mapped() const { return m_mapped; }

private:
    GpuBuffer() = default;
    ComPtr<ID3D12Resource> m_resource;
    size_t m_size = 0;
    uint8_t* m_mapped = nullptr; // 动态缓冲持久映射;CreateUpload 为 nullptr
};

} // namespace me::rhi
