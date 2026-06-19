// 精灵着色器:顶点已在 CPU 端烘入模型变换(世界像素坐标),此处只做世界→裁剪。
// uViewProj 用 row_major 接收,匹配引擎行主序存储 + 行向量约定(v' = v * M)。

cbuffer SpriteConstants : register(b0)
{
    row_major float4x4 uViewProj;
};

Texture2D    gTexture : register(t0);
SamplerState gSampler : register(s0);

struct VSInput
{
    float2 position : POSITION; // 世界像素坐标(已烘模型变换)
    float2 uv       : TEXCOORD0;
    float4 color    : COLOR;    // 每精灵 RGBA 色调
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD0;
    float4 color    : COLOR;
};

PSInput VSMain(VSInput input)
{
    PSInput o;
    o.position = mul(float4(input.position, 0.0f, 1.0f), uViewProj);
    o.uv = input.uv;
    o.color = input.color;
    return o;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return gTexture.Sample(gSampler, input.uv) * input.color;
}
