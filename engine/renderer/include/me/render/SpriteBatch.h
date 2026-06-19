#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include <d3d12.h>

#include "me/rhi/D3DCommon.h"
#include "me/core/Matrix4x4.h"
#include "me/render/SpriteDesc.h"

namespace me::rhi { class GpuDevice; class GpuBuffer; class GpuTexture; }

namespace me::render {

/// 初始可容纳精灵数;单帧提交超出时 VB/IB 自动增长到高水位(不静默丢弃)。
constexpr uint32_t kInitialSpriteCapacity = 1024;

/**
 * @brief 2D 精灵合批渲染器(M2):累积 SpriteDesc → 按纹理稳定排序 → 逐 run 合批绘制。
 *
 * 用法:Begin(viewProj) → 多次 Submit(...) → End(cmd)。模型变换在 CPU 端烘进顶点,
 * 顶点格式 pos+uv+color。调用方须已绑定 RT/视口/裁剪、已清屏、已 SetDescriptorHeaps。
 * 拥有根签名/PSO/动态顶点缓冲/静态四边形索引缓冲。
 */
class SpriteBatch {
public:
    static std::unique_ptr<SpriteBatch> Create(me::rhi::GpuDevice& device);
    ~SpriteBatch();

    void Begin(const me::Matrix4x4& viewProj);
    void Submit(const SpriteDesc& sprite);
    void End(ID3D12GraphicsCommandList* cmd);

    /// @brief 上次 End 发出的 drawcall 数(同纹理 run 合为一次)。
    size_t DrawCallCount() const { return m_drawCallCount; }

private:
    SpriteBatch() = default;
    bool EnsureCapacity(ID3D12Device* device, uint32_t spriteCount); // 不足则增长

    me::rhi::ComPtr<ID3D12RootSignature> m_rootSig;
    me::rhi::ComPtr<ID3D12PipelineState> m_pso;
    std::unique_ptr<me::rhi::GpuBuffer> m_vb;  // 动态,持久映射
    std::unique_ptr<me::rhi::GpuBuffer> m_ib;  // 静态四边形索引(位置型,R32)
    D3D12_INDEX_BUFFER_VIEW m_ibv{};
    me::rhi::ComPtr<ID3D12Device> m_device;    // 用于按需增长缓冲
    uint32_t m_capacity = 0;                   // 当前缓冲可容纳的精灵数

    me::Matrix4x4 m_viewProj{};
    std::vector<SpriteDesc> m_sprites;
    bool m_inFrame = false;
    size_t m_drawCallCount = 0;
};

} // namespace me::render
