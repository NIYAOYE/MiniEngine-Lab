#pragma once

#include <memory>
#include <d3d12.h>

#include "me/rhi/D3DCommon.h"
#include "me/core/Matrix4x4.h"

namespace me::rhi { class GpuDevice; class GpuBuffer; class GpuTexture; }

namespace me::render {

/**
 * @brief 单精灵渲染器(M1):一个根签名 + PSO + 单位四边形 VB/IB。
 *
 * Draw 假定调用方已绑定 RT/视口/裁剪、已清屏、已 SetDescriptorHeaps(SRV 堆)。
 * MVP 经根常量传入。M2 将在此之上扩展为 SpriteBatch + 正交相机。
 */
class SpriteRenderer {
public:
    static std::unique_ptr<SpriteRenderer> Create(me::rhi::GpuDevice& device);
    ~SpriteRenderer();

    void Draw(ID3D12GraphicsCommandList* cmd, const me::rhi::GpuTexture& tex,
              const me::Matrix4x4& mvp);

private:
    SpriteRenderer() = default;
    me::rhi::ComPtr<ID3D12RootSignature> m_rootSig;
    me::rhi::ComPtr<ID3D12PipelineState> m_pso;
    std::unique_ptr<me::rhi::GpuBuffer> m_vb;
    std::unique_ptr<me::rhi::GpuBuffer> m_ib;
    D3D12_VERTEX_BUFFER_VIEW m_vbv{};
    D3D12_INDEX_BUFFER_VIEW m_ibv{};
};

} // namespace me::render
