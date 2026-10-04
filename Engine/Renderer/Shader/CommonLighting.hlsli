#ifndef COMMON_LIGHTING_HLSLI
#define COMMON_LIGHTING_HLSLI

static const float PI = 3.14159265359f;

// ============================================================
// Common Lighting
// ============================================================

struct CommonLightingParameters
{
    float3 sunDirection;
    float sunIntensity;
    float3 sunColor;
    float ambientIntensity;
    float3 ambientColor;
    float padding;
};

// ============================================================
// Utility
// ============================================================

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

// ============================================================
// Sun
// ============================================================

float3 CalculateSunContribution(
    float3 direction,
    CommonLightingParameters lighting,
    float sharpness)
{
    float sunAmount = pow(
        saturate(
            dot(
                SafeNormalize(direction),
                SafeNormalize(lighting.sunDirection)
            )
        ),
        max(sharpness, 1.0f)
    );

    return GetSunRadiance(lighting) * sunAmount;
}

// ============================================================
// Tone Mapping
// ============================================================

float3 ToneMapReinhard(float3 hdrColor)
{
    hdrColor = max(hdrColor, 0.0f);
    return hdrColor / (1.0f + hdrColor);
}

// ============================================================
// Fresnel
// ============================================================

float3 FresnelSchlick(float cosTheta, float3 F0)
{
    float f = pow(1.0f - saturate(cosTheta), 5.0f);

    return F0 + (1.0f - F0) * f;
}

float3 FresnelSchlickRoughness(
    float cosTheta,
    float3 F0,
    float roughness)
{
    float3 oneMinusRoughness = 1.0f - roughness;

    return F0 + (
        max(oneMinusRoughness.xxx, F0) - F0
    ) * pow(1.0f - saturate(cosTheta), 5.0f);
}

// ============================================================
// GGX
// ============================================================

float DistributionGGX(float NdotH, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH2 = NdotH * NdotH;

    float denominator =
        NdotH2 * (a2 - 1.0f) +
        1.0f;

    return a2 / max(
        PI * denominator * denominator,
        1e-5f
    );
}

float GeometrySchlickGGX(float NdotX, float roughness)
{
    float r = roughness + 1.0f;
    float k = (r * r) / 8.0f;

    return NdotX / max(
        NdotX * (1.0f - k) + k,
        1e-5f
    );
}

float GeometrySmith(
    float NdotV,
    float NdotL,
    float roughness)
{
    return GeometrySchlickGGX(NdotV, roughness) *
           GeometrySchlickGGX(NdotL, roughness);
}

// ============================================================
// Sun Specular
// ============================================================

float3 CalculateSunSpecular(
    float3 N,
    float3 V,
    float3 L,
    float roughness,
    CommonLightingParameters lighting)
{
    float3 H = SafeNormalize(V + L);

    float NdotV = saturate(dot(N, V));
    float NdotL = saturate(dot(N, L));
    float NdotH = saturate(dot(N, H));
    float VdotH = saturate(dot(V, H));

    if (NdotL <= 0.0f)
        return 0.0f;

    float3 F0 = float3(0.020f, 0.020f, 0.020f);

    float3 F = FresnelSchlick(VdotH, F0);

    float D = DistributionGGX(
        NdotH,
        roughness
    );

    float G = GeometrySmith(
        NdotV,
        NdotL,
        roughness
    );

    float3 specular =
        (D * G * F) /
        max(
            4.0f * NdotV * NdotL,
            1e-4f
        );

    return specular *
           GetSunRadiance(lighting) *
           NdotL;
}

// ============================================================
// TBN
// ============================================================

void BuildTBN(
    float3 normal,
    float3 tangent,
    out float3 N,
    out float3 T,
    out float3 B)
{
    N = SafeNormalize(normal);

    T = SafeNormalize(
        tangent -
        N * dot(tangent, N)
    );

    B = SafeNormalize(
        cross(N, T)
    );
}

// ============================================================
// Procedural Atmospheric Sky
//
// HDRIをベースにしつつ、
// 空の高さによる大気色を加える。
// ============================================================

float3 GetSkyColor(float3 direction, CommonLightingParameters lighting)
{
    direction = SafeNormalize(direction);

    float height = saturate(direction.y);

    // Horizon
    float3 horizonColor = float3(
        0.58f,
        0.74f,
        0.91f
    );

    // Zenith
    float3 zenithColor = float3(
        0.055f,
        0.19f,
        0.48f
    );

    float skyGradient = pow(height, 0.42f);

    float3 sky = lerp(
        horizonColor,
        zenithColor,
        skyGradient
    );

    // 地平線付近の大気散乱
    float horizon = pow(
        1.0f - saturate(abs(direction.y)),
        3.0f
    );

    sky += float3(
        0.06f,
        0.08f,
        0.10f
    ) * horizon;

    return sky;
}

// ============================================================
// Cloud Phase
//
// SkyGLのdual-lobe HGを参考。
// 前方散乱と後方散乱を混ぜる。
// ============================================================

float HenyeyGreenstein(float cosTheta, float g)
{
    float g2 = g * g;

    float denominator =
        1.0f +
        g2 -
        2.0f * g * cosTheta;

    return (1.0f - g2) / (
        4.0f *
        PI *
        pow(
            max(denominator, 1e-4f),
            1.5f
        )
    );
}

float DualLobeCloudPhase(float cosTheta)
{
    // 前方散乱
    float forward = HenyeyGreenstein(
        cosTheta,
        0.65f
    );

    // 後方散乱
    float backward = HenyeyGreenstein(
        cosTheta,
        -0.20f
    );

    float phase =
        forward * 0.82f +
        backward * 0.18f;

    // 過剰なハイライトを防止
    return saturate(phase * 1.8f);
}

// ============================================================
// Cloud Multiple Scattering Approximation
//
// 4 octave attenuation。
// SkyGLの考え方を簡略化。
// ============================================================

float CloudMultipleScattering(float opticalDepth)
{
    float attenuation = 0.0f;

    attenuation += exp(
        -opticalDepth * 0.25f
    ) * 0.45f;

    attenuation += exp(
        -opticalDepth * 0.50f
    ) * 0.30f;

    attenuation += exp(
        -opticalDepth * 1.00f
    ) * 0.18f;

    attenuation += exp(
        -opticalDepth * 2.00f
    ) * 0.07f;

    return saturate(attenuation);
}

#endif