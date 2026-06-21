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

float4 PS(PS_INPUT input) : SV_TARGET
{
    float3 normal = normalize(input.normal);
    
    // 疑似的なカメラ位置と太陽(ライト)の方向の設定
    float3 viewDir = normalize(float3(0.0f, 10.0f, -20.0f) - input.worldPos);
    float3 lightDir = normalize(float3(1.0f, 0.8f, 1.0f));

    // 水の基本色
    float3 deepWaterColor = float3(0.01f, 0.1f, 0.2f);
    float3 shallowWaterColor = float3(0.1f, 0.4f, 0.5f);
    
    // 波の高さ(Y座標)や法線をもとに水の色をブレンド
    float diffuse = max(0.0f, dot(normal, lightDir));
    float3 waterColor = lerp(deepWaterColor, shallowWaterColor, diffuse);
    
    // スペキュラ（波頭の鋭い太陽の反射）
    float3 halfVector = normalize(lightDir + viewDir);
    float spec = pow(max(0.0f, dot(normal, halfVector)), 300.0f); // 300で鋭いハイライトにする
    float3 specularColor = float3(1.0f, 1.0f, 1.0f) * spec * 2.5f;

    // フレネル効果（視線と水面の角度による空の反射）
    float fresnel = pow(1.0f - max(0.0f, dot(normal, viewDir)), 4.0f);
    float3 skyColor = float3(0.5f, 0.7f, 0.9f); // 空の水色
    
    // 現在のテクスチャをうっすら波の歪みとして混ぜる [cite: 3]
    float2 distortedTexcoord = input.texcoord + normal.xz * 0.1f;
    float4 texColor = gDiffuse.Sample(gSampler, distortedTexcoord);

    // すべてを合成
    float3 finalColor = waterColor + (skyColor * fresnel) + specularColor + (texColor.rgb * 0.1f);
    
    // 遠くの波がチラつかないように軽いフォグをかけるとよりリアルになります
    float distance = length(float3(0.0f, 10.0f, -20.0f) - input.worldPos);
    float fogFactor = clamp((distance - 10.0f) / 100.0f, 0.0f, 1.0f);
    finalColor = lerp(finalColor, skyColor, fogFactor);

    return float4(finalColor, 1.0f);
}