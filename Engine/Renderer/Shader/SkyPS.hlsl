Texture2D gSkyTexture : register(t0);
SamplerState gSampler : register(s0);

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 worldPos : TEXCOORD1;
};

// Sky Sphere Pixel Shader
// HDR/EXRテクスチャをサンプリングして出力
float4 main(PS_INPUT input) : SV_TARGET
{
    // UV座標から天空テクスチャをサンプリング
    float3 color = gSkyTexture.Sample(gSampler, input.texcoord).rgb;
    
    // リニアカラー空間で返す（フレームバッファがHDR対応の場合）
    return float4(color, 1.0f);
}
