#include "me/render/SpriteBatch.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "me/rhi/GpuDevice.h"
#include "me/rhi/GpuBuffer.h"
#include "me/rhi/GpuTexture.h"
#include "me/rhi/Shader.h"
#include "me/core/Assert.h"
#include "me/core/Log.h"

namespace me::render {

namespace {
using namespace me::rhi;

constexpr uint32_t kViewProjConstantCount = 16; // 4x4 float
constexpr uint32_t kRootParamViewProj = 0;
constexpr uint32_t kRootParamTexture = 1;
constexpr uint32_t kVertsPerSprite = 4;
constexpr uint32_t kIndicesPerSprite = 6;

/// 顶点:世界像素坐标 + UV + RGBA 色调。布局须与 sprite.hlsl 输入一致。
struct BatchVertex {
    float x, y;
    float u, v;
    float r, g, b, a;
};

std::wstring ShaderPath() {
    const std::string p = std::string(ME_ASSET_DIR) + "/shaders/sprite.hlsl";
    return std::wstring(p.begin(), p.end());
}

/// 为 spriteCount 个精灵生成位置型四边形索引:精灵 i 引用顶点 i*4 + {0,1,2,0,2,3}。
std::vector<uint32_t> BuildQuadIndices(uint32_t spriteCount) {
    std::vector<uint32_t> indices(static_cast<size_t>(spriteCount) * kIndicesPerSprite);
    for (uint32_t i = 0; i < spriteCount; ++i) {
        const uint32_t base = i * kVertsPerSprite;
        uint32_t* dst = &indices[static_cast<size_t>(i) * kIndicesPerSprite];
        dst[0] = base + 0; dst[1] = base + 1; dst[2] = base + 2;
        dst[3] = base + 0; dst[4] = base + 2; dst[5] = base + 3;
    }
    return indices;
}

/// 把一个 SpriteDesc 烘成 4 个世界空间顶点(含旋转、UV、色调)。
void BuildQuad(const SpriteDesc& s, BatchVertex out[kVertsPerSprite]) {
    const float hw = s.dstRect.width * 0.5f;
    const float hh = s.dstRect.height * 0.5f;
    const float cx = s.dstRect.x + hw;
    const float cy = s.dstRect.y + hh;
    const float cs = std::cos(s.rotation);
    const float sn = std::sin(s.rotation);
    // 局部角点 (lx,ly) 绕中心旋转(行向量:[lx,ly]*R = (lx*c - ly*s, lx*s + ly*c))。
    auto place = [&](float lx, float ly, float& ox, float& oy) {
        ox = cx + (lx * cs - ly * sn);
        oy = cy + (lx * sn + ly * cs);
    };
    const float uMin = s.srcRect.x;
    const float uMax = s.srcRect.x + s.srcRect.width;
    const float vMin = s.srcRect.y;                  // 上
    const float vMax = s.srcRect.y + s.srcRect.height; // 下
    const me::Vector4& c = s.color;
    // 世界 Y 向上、纹理 V 向下:底边(-hh)取 vMax,顶边(+hh)取 vMin。
    struct Corner { float lx, ly, u, v; };
    const Corner corners[kVertsPerSprite] = {
        {-hw, -hh, uMin, vMax}, // 0 左下
        { hw, -hh, uMax, vMax}, // 1 右下
        { hw,  hh, uMax, vMin}, // 2 右上
        {-hw,  hh, uMin, vMin}, // 3 左上
    };
    for (uint32_t i = 0; i < kVertsPerSprite; ++i) {
        place(corners[i].lx, corners[i].ly, out[i].x, out[i].y);
        out[i].u = corners[i].u;
        out[i].v = corners[i].v;
        out[i].r = c.x; out[i].g = c.y; out[i].b = c.z; out[i].a = c.w;
    }
}
} // namespace

std::unique_ptr<SpriteBatch> SpriteBatch::Create(GpuDevice& device) {
    auto self = std::unique_ptr<SpriteBatch>(new SpriteBatch());
    ID3D12Device* dev = device.Device();
    self->m_device = dev;

    // 1) 着色器(FXC SM5.1)。
    ComPtr<ID3DBlob> vs = CompileHlsl(ShaderPath(), "VSMain", "vs_5_1");
    ComPtr<ID3DBlob> ps = CompileHlsl(ShaderPath(), "PSMain", "ps_5_1");
    if (!vs || !ps) return nullptr;

    // 2) 根签名:b0 = 16 根常量(viewProj);t0 = SRV 表;s0 = 静态采样器。
    D3D12_DESCRIPTOR_RANGE srvRange = {};
    srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRange.NumDescriptors = 1;
    srvRange.BaseShaderRegister = 0; // t0

    D3D12_ROOT_PARAMETER params[2] = {};
    params[kRootParamViewProj].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    params[kRootParamViewProj].Constants.Num32BitValues = kViewProjConstantCount;
    params[kRootParamViewProj].Constants.ShaderRegister = 0; // b0
    params[kRootParamViewProj].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    params[kRootParamTexture].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
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

    // 3) 输入布局 + PSO(POSITION/TEXCOORD/COLOR)。
    D3D12_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso = {};
    pso.pRootSignature = self->m_rootSig.Get();
    pso.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
    pso.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
    pso.InputLayout = {layout, 3};
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.NumRenderTargets = 1;
    pso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pso.SampleDesc.Count = 1;
    pso.SampleMask = UINT_MAX;
    pso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pso.RasterizerState.DepthClipEnable = TRUE;
    // 标准 src-over alpha 混合:精灵透明区(alpha=0)让背景透出;不透明精灵/瓦片
    // (alpha=1)等同直接覆盖,故对既有不透明内容无视觉变化。农场精灵需要透明背景。
    for (auto& rt : pso.BlendState.RenderTarget) {
        rt.BlendEnable = TRUE;
        rt.SrcBlend = D3D12_BLEND_SRC_ALPHA;
        rt.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        rt.BlendOp = D3D12_BLEND_OP_ADD;
        rt.SrcBlendAlpha = D3D12_BLEND_ONE;
        rt.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
        rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    }
    pso.DepthStencilState.DepthEnable = FALSE;
    pso.DepthStencilState.StencilEnable = FALSE;

    if (FAILED(dev->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&self->m_pso)))) {
        return nullptr;
    }

    // 4) 初始动态 VB + 静态 IB(容量 kInitialSpriteCapacity)。
    if (!self->EnsureCapacity(dev, kInitialSpriteCapacity)) return nullptr;
    return self;
}

SpriteBatch::~SpriteBatch() = default;

bool SpriteBatch::EnsureCapacity(ID3D12Device* device, uint32_t spriteCount) {
    if (spriteCount <= m_capacity) return true;
    // 增长到请求容量(高水位,不回缩)。
    m_vb = GpuBuffer::CreateDynamic(
        device, static_cast<size_t>(spriteCount) * kVertsPerSprite * sizeof(BatchVertex));
    const std::vector<uint32_t> indices = BuildQuadIndices(spriteCount);
    m_ib = GpuBuffer::CreateUpload(device, indices.data(),
                                   indices.size() * sizeof(uint32_t));
    if (!m_vb || !m_ib) {
        ME_LOG_ERROR("SpriteBatch: 顶点/索引缓冲增长失败");
        m_capacity = 0;
        return false;
    }
    m_ibv.BufferLocation = m_ib->Gpu();
    m_ibv.SizeInBytes = static_cast<UINT>(m_ib->Size());
    m_ibv.Format = DXGI_FORMAT_R32_UINT;
    m_capacity = spriteCount;
    return true;
}

void SpriteBatch::Begin(const me::Matrix4x4& viewProj) {
    ME_ASSERT_MSG(!m_inFrame, "SpriteBatch::Begin: 未配对的 Begin/End");
    m_inFrame = true;
    m_viewProj = viewProj;
    m_sprites.clear();
    m_drawCallCount = 0;
}

void SpriteBatch::Submit(const SpriteDesc& sprite) {
    ME_ASSERT_MSG(m_inFrame, "SpriteBatch::Submit: 必须在 Begin 与 End 之间调用");
    if (sprite.texture == nullptr) {
        ME_LOG_WARN("SpriteBatch::Submit: 跳过纹理为空的精灵");
        return;
    }
    m_sprites.push_back(sprite);
}

void SpriteBatch::End(ID3D12GraphicsCommandList* cmd) {
    ME_ASSERT_MSG(m_inFrame, "SpriteBatch::End: 未配对的 Begin/End");
    m_inFrame = false;
    m_drawCallCount = 0;
    if (m_sprites.empty()) return;

    if (!EnsureCapacity(m_device.Get(), static_cast<uint32_t>(m_sprites.size()))) return;

    // 按纹理指针稳定排序:同纹理相邻 → 合为一次 drawcall。
    std::stable_sort(m_sprites.begin(), m_sprites.end(),
                     [](const SpriteDesc& a, const SpriteDesc& b) {
                         return a.texture < b.texture;
                     });

    // 烘顶点并写入动态 VB。
    std::vector<BatchVertex> verts(m_sprites.size() * kVertsPerSprite);
    for (size_t i = 0; i < m_sprites.size(); ++i) {
        BuildQuad(m_sprites[i], &verts[i * kVertsPerSprite]);
    }
    const size_t vbBytes = verts.size() * sizeof(BatchVertex);
    m_vb->Write(verts.data(), vbBytes, 0);

    // 公共绑定。
    D3D12_VERTEX_BUFFER_VIEW vbv = {};
    vbv.BufferLocation = m_vb->Gpu();
    vbv.SizeInBytes = static_cast<UINT>(vbBytes);
    vbv.StrideInBytes = sizeof(BatchVertex);

    cmd->SetPipelineState(m_pso.Get());
    cmd->SetGraphicsRootSignature(m_rootSig.Get());
    cmd->SetGraphicsRoot32BitConstants(kRootParamViewProj, kViewProjConstantCount,
                                       &m_viewProj, 0);
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmd->IASetVertexBuffers(0, 1, &vbv);
    cmd->IASetIndexBuffer(&m_ibv);

    // 逐纹理 run:[runStart, runEnd) 同纹理 → 一次 DrawIndexedInstanced。
    size_t runStart = 0;
    while (runStart < m_sprites.size()) {
        const me::rhi::GpuTexture* tex = m_sprites[runStart].texture;
        size_t runEnd = runStart;
        while (runEnd < m_sprites.size() && m_sprites[runEnd].texture == tex) ++runEnd;
        const UINT count = static_cast<UINT>(runEnd - runStart);
        cmd->SetGraphicsRootDescriptorTable(kRootParamTexture, tex->SrvGpu());
        // 位置型索引:StartIndexLocation 已指向 runStart*4 起的顶点,BaseVertex=0。
        cmd->DrawIndexedInstanced(count * kIndicesPerSprite, 1,
                                  static_cast<UINT>(runStart) * kIndicesPerSprite, 0, 0);
        ++m_drawCallCount;
        runStart = runEnd;
    }
}

} // namespace me::render
