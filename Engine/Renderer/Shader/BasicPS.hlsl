cbuffer MatrixBuffer : register(b0)
{
    float4x4 mvp;
    float time;
    float3 padding;
    float3 cameraPos;
    float pad2;
};

Texture2D gDiffuse : register(t0);
SamplerState gSampler : register(s0);

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float time : TEXCOORD1;
    float3 worldPos : TEXCOORD2;
    float3 normal : NORMAL;
};

// ピクセルシェーダ: 波のライティング（ディフューズ、スペキュラ、フレネル）と距離フォグを合成して出力
float4 PS(PS_INPUT input) : SV_TARGET
{
    // 法線とカメラ・ライト方向を取得
    float3 N = normalize(input.normal);
    float3 V = normalize(cameraPos - input.worldPos); // 視線方向（カメラからピクセルへのベクトル）
    float3 L = normalize(float3(1.0f, 0.8f, 1.0f)); // 太陽（ライト）方向（固定）

    // ベースの水色（深浅）を法線の傾きで補間
    float3 deepWaterColor = float3(0.01f, 0.08f, 0.14f);
    float3 shallowWaterColor = float3(0.08f, 0.42f, 0.56f);
    float NdotL = saturate(dot(N, L));
    float3 baseColor = lerp(deepWaterColor, shallowWaterColor, NdotL);

    // テクスチャを軽く混ぜる（法線による歪み + 時間でわずかに動かす）
    float2 waveScroll = float2(time * 0.01f, time * -0.007f);
    float2 distortedUV = input.texcoord + N.xz * 0.03f + waveScroll;
    float3 tex = gDiffuse.Sample(gSampler, distortedUV).rgb;

    // スペキュラ（Blinn-Phong）
    float3 H = normalize(L + V);
    float specPow = 200.0f; // ハイライトの鋭さ
    float specIntensity = 3.0f;
    float spec = pow(saturate(dot(N, H)), specPow) * specIntensity;

    // フレネル（視角依存の反射）
    float fresnel = pow(1.0f - saturate(dot(N, V)), 4.0f);

    // 空色（フレネルと距離に混ぜるための色）
    float3 skyColor = float3(0.45f, 0.68f, 0.9f);

    // 合成: ベース + テクスチャの薄い乗算 + スペキュラ、フレネルで空を反射
    float3 color = baseColor * 0.85f + tex * 0.15f + spec;
    color = lerp(color, skyColor, fresnel * 0.9f);

    // 距離フォグ（遠景の波が消えすぎないように開始距離を遠めに設定）
    float distanceToCamera = length(cameraPos - input.worldPos);
    const float fogStart = 600.0f;
    const float fogEnd = 3000.0f;
    float fogFactor = saturate((distanceToCamera - fogStart) / (fogEnd - fogStart));
    color = lerp(color, skyColor, fogFactor);

    // 出力（線形色空間）
    return float4(color, 1.0f);
}