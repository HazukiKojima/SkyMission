#ifndef COMMON_LIGHTING_HLSLI
#define COMMON_LIGHTING_HLSLI

static const float PI = 3.14159265359f;

// 共通の照明パラメーター

struct CommonLightingParameters
{
    float3 sunDirection;
    float sunIntensity;
    float3 sunColor;
    float ambientIntensity;
    float3 ambientColor;
    float padding;
};

// 共通の計算補助

float3 SafeNormalize(float3 v)
{
    float lengthSquared = dot(v, v);
    return v * rsqrt(max(lengthSquared, 1e-6f));
}

float3 GetSunRadiance(CommonLightingParameters lighting)
{
    return max(lighting.sunColor, 0.0f) * max(lighting.sunIntensity, 0.0f);
}

float3 GetAmbientRadiance(CommonLightingParameters lighting)
{
    return max(lighting.ambientColor, 0.0f) * max(lighting.ambientIntensity, 0.0f);
}

// 太陽光の寄与

float3 CalculateSunContribution(float3 direction, CommonLightingParameters lighting, float sharpness)
{
    float sunAmount = pow(
        saturate(dot(SafeNormalize(direction), SafeNormalize(lighting.sunDirection))), max(sharpness, 1.0f));

    return GetSunRadiance(lighting) * sunAmount;
}

// HDR色を表示範囲へ圧縮する

float3 ToneMapReinhard(float3 hdrColor)
{
    hdrColor = max(hdrColor, 0.0f);
    return hdrColor / (1.0f + hdrColor);
}

// フレネル反射

float3 FresnelSchlick(float cosTheta, float3 F0)
{
    float f = pow(1.0f - saturate(cosTheta), 5.0f);

    return F0 + (1.0f - F0) * f;
}

float3 FresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
    float3 oneMinusRoughness = 1.0f - roughness;

    return F0 + (max(oneMinusRoughness.xxx, F0) - F0) * pow(1.0f - saturate(cosTheta), 5.0f);
}

// GGXによる微小面の分布と遮蔽

float DistributionGGX(float NdotH, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH2 = NdotH * NdotH;

    float denominator = NdotH2 * (a2 - 1.0f) + 1.0f;

    return a2 / max(PI * denominator * denominator, 1e-5f);
}

float GeometrySchlickGGX(float NdotX, float roughness)
{
    float r = roughness + 1.0f;
    float k = (r * r) / 8.0f;

    return NdotX / max(NdotX * (1.0f - k) + k, 1e-5f);
}

float GeometrySmith(float NdotV, float NdotL, float roughness)
{
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

// 太陽光の鏡面反射

float3 CalculateSunSpecular(float3 N, float3 V, float3 L, float roughness, CommonLightingParameters lighting)
{
    float3 H = SafeNormalize(V + L);

    float NdotV = saturate(dot(N, V));
    float NdotL = saturate(dot(N, L));
    float NdotH = saturate(dot(N, H));
    float VdotH = saturate(dot(V, H));

    // 背面の反射はゼロとし、分岐を抜けた後の戻り値を明示する
    float3 contribution = 0.0f;
    if (NdotL > 0.0f)
    {

        float3 F0 = float3(0.020f, 0.020f, 0.020f);

        float3 F = FresnelSchlick(VdotH, F0);

        float D = DistributionGGX(NdotH, roughness);

        float G = GeometrySmith(NdotV, NdotL, roughness);

        float3 specular = (D * G * F) / max(4.0f * NdotV * NdotL, 1e-4f);

        contribution = specular * GetSunRadiance(lighting) * NdotL;
    }
    return contribution;
}

// 接線空間の基底を構築する

void BuildTBN(float3 normal, float3 tangent, out float3 N, out float3 T, out float3 B)
{
    N = SafeNormalize(normal);

    T = SafeNormalize(tangent - N * dot(tangent, N));

    B = SafeNormalize(cross(N, T));
}

// 視線の高さで空の色を補間し、地平線付近の散乱光を加える

float3 GetSkyColor(float3 direction, CommonLightingParameters lighting)
{
    direction = SafeNormalize(direction);

    float height = saturate(direction.y);

    // 地平線の色
    float3 horizonColor = float3(0.68f, 0.82f, 0.98f);

    // 天頂の色
    float3 zenithColor = float3(0.07f, 0.23f, 0.54f);

    float skyGradient = pow(height, 0.42f);

    float3 sky = lerp(horizonColor, zenithColor, skyGradient);

    // 地平線付近の大気散乱
    float horizon = pow(1.0f - saturate(abs(direction.y)), 3.0f);

    sky += float3(0.06f, 0.08f, 0.10f) * horizon;

    return sky;
}

#endif
