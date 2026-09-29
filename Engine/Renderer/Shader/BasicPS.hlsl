cbuffer MatrixBuffer : register(b0)
{
    float4x4 mvp;
    float time;
    float3 padding;
    float3 cameraPos;
    float pad2;
    float3 sunDirection;
    float sunIntensity;
    float3 sunColor;
    float ambientIntensity;
    float3 ambientColor;
    float pad3;
};

Texture2D gDiffuse : register(t0);
Texture2D gOceanNormal : register(t1);
SamplerState gSampler : register(s0);

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float time : TEXCOORD1;
    float3 worldPos : TEXCOORD2;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
};

// 水面のライティング計算とフォグ処理
float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0f - F0) * pow(1.0f - saturate(cosTheta), 5.0f);
}

float DistributionGGX(float NdotH, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float d = NdotH * NdotH * (a2 - 1.0f) + 1.0f;
    return a2 / max(3.14159265f * d * d, 0.0001f);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float k = (roughness + 1.0f) * (roughness + 1.0f) / 8.0f;
    return NdotV / max(NdotV * (1.0f - k) + k, 0.0001f);
}

float GeometrySmith(float NdotV, float NdotL, float roughness)
{
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

float4 PS(PS_INPUT input) : SV_TARGET
{
    // 法線と視線・光方向の計算
    float3 N = normalize(input.normal);
    float3 V = normalize(cameraPos - input.worldPos);
    float3 L = normalize(sunDirection);

    // 深浅のベース色を太陽方向に応じて補間
    float3 deepWaterColor = float3(0.01f, 0.08f, 0.14f);
    float3 shallowWaterColor = float3(0.03f, 0.16f, 0.24f);
    float NdotL = saturate(dot(N, L));
    float3 baseColor = lerp(deepWaterColor, shallowWaterColor, NdotL * 0.25f);

    // 法線マップで細かい凹凸を合成
    float2 normalUV = input.worldPos.xz * 0.035f + float2(time * 0.018f, time * -0.011f);
    float3 tangent = normalize(input.tangent - N * dot(input.tangent, N));
    float3 bitangent = normalize(cross(N, tangent));
    float3 mappedNormal = gOceanNormal.Sample(gSampler, normalUV).xyz * 2.0f - 1.0f;
    mappedNormal.xy *= 0.55f;
    N = normalize(tangent * mappedNormal.x + bitangent * mappedNormal.y + N * mappedNormal.z);
    float3 R = reflect(-V, N);
    NdotL = saturate(dot(N, L));
    baseColor = lerp(deepWaterColor, shallowWaterColor, NdotL * 0.25f);

    // テクスチャを重ねて水面の模様を作る
    float2 waveScroll = float2(time * 0.012f, time * -0.008f);
    float2 waterUV = input.worldPos.xz * 0.018f;
    float3 texA = gDiffuse.Sample(gSampler, waterUV + N.xz * 0.035f + waveScroll).rgb;
    float3 texB = gDiffuse.Sample(gSampler, waterUV * 0.63f - N.zx * 0.02f - waveScroll * 0.7f).rgb;
    float3 tex = lerp(texA, texB, 0.35f);

    // スペキュラとディフューズの計算
    float3 waterF0 = float3(0.02f, 0.02f, 0.02f);
    float roughness = 0.16f;
    float NdotV = saturate(dot(N, V));
    float3 H = normalize(V + L);
    float NdotH = saturate(dot(N, H));
    float VdotH = saturate(dot(V, H));
    float3 fresnel = FresnelSchlick(VdotH, waterF0);
    float distribution = DistributionGGX(NdotH, roughness);
    float geometry = GeometrySmith(NdotV, NdotL, roughness);
    float3 specular = (distribution * geometry * fresnel) / max(4.0f * NdotV * NdotL, 0.001f);
    float3 directDiffuse = baseColor * (1.0f - fresnel) * NdotL * sunIntensity;

    // 環境反射とフォグ
    float3 skyColor = float3(0.45f, 0.68f, 0.9f);
    float3 ambient = ambientColor * ambientIntensity * lerp(0.65f, 1.0f, saturate(N.y));
    float3 reflectedSky = lerp(float3(0.18f, 0.30f, 0.42f), skyColor, saturate(0.5f + 0.5f * R.y));
    float3 environmentReflection = reflectedSky * fresnel * (0.65f + 0.35f * saturate(R.y));
    float3 color = ambient + directDiffuse + sunColor * sunIntensity * specular + environmentReflection + baseColor * 0.05f + tex * 0.10f;
    float worldSkyLight = smoothstep(0.0f, 1.0f, saturate(N.y)) * 0.08f;
    color += skyColor * worldSkyLight;

    float foam = smoothstep(0.78f, 0.96f, 1.0f - N.y);
    color = lerp(color, float3(0.72f, 0.82f, 0.86f), foam * 0.12f);

    // フォグで遠景を薄める
    float distanceToCamera = length(cameraPos - input.worldPos);
    const float fogStart = 600.0f;
    const float fogEnd = 3000.0f;
    float fogFactor = saturate((distanceToCamera - fogStart) / (fogEnd - fogStart));
    float horizonFactor = pow(1.0f - saturate(abs(V.y)), 2.0f);
    fogFactor = max(fogFactor, horizonFactor * 0.35f);
    color = lerp(color, skyColor, fogFactor);

    // HDR範囲に収める
    color = color / (1.0f + color);
    return float4(saturate(color), 1.0f);
}