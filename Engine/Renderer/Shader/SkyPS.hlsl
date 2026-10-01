Texture2D gSkyTexture : register(t0);
SamplerState gSampler : register(s0);

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 worldPos : TEXCOORD1;
};

// 空球用ピクセルシェーダー
// HDR/EXRテクスチャをサンプリングし、表示色に変換
float4 main(PS_INPUT input) : SV_TARGET
{
    // 球面UVで空テクスチャを参照
    float3 color = gSkyTexture.Sample(gSampler, input.texcoord).rgb;
    
    // HDR色を表示可能な範囲にトーンマッピング
    color = color / (1.0f + color);
    return float4(max(color, 0.0f), 1.0f);
}
