Texture2D gSkyTexture : register(t0);
SamplerState gSampler : register(s0);

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 worldPos : TEXCOORD1;
};

float4 main(PS_INPUT input) : SV_TARGET
{
    // テクスチャのサンプリング
    float3 rawColor = gSkyTexture.Sample(gSampler, input.texcoord).rgb;
    
    // 露出とベースの明るい空色のブレンド
    float exposure = 1.0f;
    float3 color = rawColor * exposure;
    
    // 太陽と反対側が暗く沈むのを防ぐため、明るい青を加算
    color = max(color, float3(0.22f, 0.40f, 0.65f));

    // トーンマッピング
    color = color / (1.0f + color);
    
    return float4(max(color, 0.0f), 1.0f);
}
