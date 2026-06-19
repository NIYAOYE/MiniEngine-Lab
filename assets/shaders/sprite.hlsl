// 精灵着色器:把单位四边形经 MVP 变换后采样纹理。
// MVP 用 row_major 接收,匹配引擎的行主序存储 + 行向量约定(v' = v * M)。

cbuffer SpriteConstants : register(b0)
{
    row_major float4x4 uMVP;
};

Texture2D    gTexture : register(t0);
SamplerState gSampler : register(s0);

struct VSInput
{
    float2 position : POSITION; // 局部空间 ±0.5
    float2 uv       : TEXCOORD0;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD0;
};

PSInput VSMain(VSInput input)
{
    PSInput o;
    // 行向量:把局部点当作行向量左乘 MVP。
    o.position = mul(float4(input.position, 0.0f, 1.0f), uMVP);
    o.uv = input.uv;
    return o;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return gTexture.Sample(gSampler, input.uv);
}
