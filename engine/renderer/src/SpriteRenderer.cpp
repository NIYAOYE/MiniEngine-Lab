#include "me/render/SpriteRenderer.h"

#include <string>
#include "me/rhi/GpuDevice.h"
#include "me/rhi/GpuBuffer.h"
#include "me/rhi/GpuTexture.h"
#include "me/rhi/Shader.h"
#include "me/rhi/QuadGeometry.h"

namespace me::render {

namespace {
using namespace me::rhi;

constexpr uint32_t kMvpConstantCount = 16; // 4x4 float
constexpr uint32_t kRootParamMvp = 0;
constexpr uint32_t kRootParamTexture = 1;

std::wstring ShaderPath() {
    // ME_ASSET_DIR 由 CMake 注入(仓库 assets 绝对路径)。
    const std::string p = std::string(ME_ASSET_DIR) + "/shaders/sprite.hlsl";
    return std::wstring(p.begin(), p.end());
}
} // namespace

std::unique_ptr<SpriteRenderer> SpriteRenderer::Create(GpuDevice& device) {
    auto self = std::unique_ptr<SpriteRenderer>(new SpriteRenderer());
    ID3D12Device* dev = device.Device();

    // 1) 着色器。
    ComPtr<ID3DBlob> vs = CompileHlsl(ShaderPath(), "VSMain", "vs_5_1");
    ComPtr<ID3DBlob> ps = CompileHlsl(ShaderPath(), "PSMain", "ps_5_1");
    if (!vs || !ps) return nullptr;

    // 2) 根签名:b0 = 16 个根常量(MVP);t0 = SRV 描述符表;s0 = 静态采样器。
    D3D12_DESCRIPTOR_RANGE srvRange = {};
    srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRange.NumDescriptors = 1;
    srvRange.BaseShaderRegister = 0; // t0

    D3D12_ROOT_PARAMETER params[2] = {};
    params[kRootParamMvp].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    params[kRootParamMvp].Constants.Num32BitValues = kMvpConstantCount;
    params[kRootParamMvp].Constants.ShaderRegister = 0; // b0
    params[kRootParamMvp].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    params[kRootParamTexture].ParameterType =
        D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[kRootParamTexture].DescriptorTable.NumDescriptorRanges = 1;
    params[kRootParamTexture].DescriptorTable.pDescriptorRanges = &srvRange;
    params[kRootParamTexture].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT; // 点采样,回读确定
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.ShaderRegister = 0; // s0
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
    rsDesc.NumParameters = 2;
    rsDesc.pParameters = params;
    rsDesc.NumStaticSamplers = 1;
    rsDesc.pStaticSamplers = &sampler;
    rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> rsBlob, rsErr;
    if (FAILED(D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1,
                                           &rsBlob, &rsErr))) {
        return nullptr;
    }
    if (FAILED(dev->CreateRootSignature(0, rsBlob->GetBufferPointer(),
                                        rsBlob->GetBufferSize(),
                                        IID_PPV_ARGS(&self->m_rootSig)))) {
        return nullptr;
    }

    // 3) 输入布局 + PSO。
    D3D12_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso = {};
    pso.pRootSignature = self->m_rootSig.Get();
    pso.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
    pso.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
    pso.InputLayout = {layout, 2};
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.NumRenderTargets = 1;
    pso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pso.SampleDesc.Count = 1;
    pso.SampleMask = UINT_MAX;
    // 光栅化:M1 关背面剔除,绕序无关(学习优先)。
    pso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pso.RasterizerState.DepthClipEnable = TRUE;
    // 混合:不透明直写;深度/模板关闭。
    for (auto& rt : pso.BlendState.RenderTarget) {
        rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    }
    pso.DepthStencilState.DepthEnable = FALSE;
    pso.DepthStencilState.StencilEnable = FALSE;

    if (FAILED(dev->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&self->m_pso)))) {
        return nullptr;
    }

    // 4) 单位四边形 VB/IB(上传堆)。
    const auto verts = UnitQuadVertices();
    const auto indices = UnitQuadIndices();
    self->m_vb = GpuBuffer::CreateUpload(dev, verts.data(),
                                         verts.size() * sizeof(SpriteVertex));
    self->m_ib = GpuBuffer::CreateUpload(dev, indices.data(),
                                         indices.size() * sizeof(uint16_t));
    if (!self->m_vb || !self->m_ib) return nullptr;

    self->m_vbv.BufferLocation = self->m_vb->Gpu();
    self->m_vbv.SizeInBytes = static_cast<UINT>(self->m_vb->Size());
    self->m_vbv.StrideInBytes = sizeof(SpriteVertex);
    self->m_ibv.BufferLocation = self->m_ib->Gpu();
    self->m_ibv.SizeInBytes = static_cast<UINT>(self->m_ib->Size());
    self->m_ibv.Format = DXGI_FORMAT_R16_UINT;
    return self;
}

SpriteRenderer::~SpriteRenderer() = default;

void SpriteRenderer::Draw(ID3D12GraphicsCommandList* cmd,
                          const me::rhi::GpuTexture& tex,
                          const me::Matrix4x4& mvp) {
    cmd->SetPipelineState(m_pso.Get());
    cmd->SetGraphicsRootSignature(m_rootSig.Get());
    // 行主序 m[4][4] 的内存布局即 row0..row3,与 HLSL row_major 一致,直接灌入。
    cmd->SetGraphicsRoot32BitConstants(0, 16, &mvp, 0);
    cmd->SetGraphicsRootDescriptorTable(1, tex.SrvGpu());
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmd->IASetVertexBuffers(0, 1, &m_vbv);
    cmd->IASetIndexBuffer(&m_ibv);
    cmd->DrawIndexedInstanced(me::rhi::kSpriteIndexCount, 1, 0, 0, 0);
}

} // namespace me::render
