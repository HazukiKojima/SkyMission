#ifndef COMMON_LIGHTING_HLSLI
#define COMMON_LIGHTING_HLSLI

static const float PI = 3.14159265359f;

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
    return v * rsqrt(max(dot(v, v), 1e-6f));
}

float3 GetSunRadiance(CommonLightingParameters lighting)
{
    return max(lighting.sunColor, 0.0f) *
           max(lighting.sunIntensity, 0.0f);
}

float3 GetAmbientRadiance(CommonLightingParameters lighting)
{
    return max(lighting.ambientColor, 0.0f) *
           max(lighting.ambientIntensity, 0.0f);
}

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
    float f = pow(
        1.0f - saturate(cosTheta),
        5.0f
    );

    return F0 + (1.0f - F0) * f;
}

float3 FresnelSchlickRoughness(
    float cosTheta,
    float3 F0,
    float roughness)
{
    float3 oneMinusRoughness =
        1.0f - roughness;

    return F0 +
        (
            max(
                oneMinusRoughness.xxx,
                F0
            ) - F0
        ) *
        pow(
            1.0f - saturate(cosTheta),
            5.0f
        );
}

// ============================================================
// GGX
// ============================================================

float DistributionGGX(
    float NdotH,
    float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;

    float NdotH2 =
        NdotH * NdotH;

    float denom =
        NdotH2 * (a2 - 1.0f) +
        1.0f;

    return a2 /
        max(
            PI * denom * denom,
            1e-5f
        );
}

float GeometrySchlickGGX(
    float NdotX,
    float roughness)
{
    float r =
        roughness + 1.0f;

    float k =
        (r * r) / 8.0f;

    return NdotX /
        max(
            NdotX * (1.0f - k) + k,
            1e-5f
        );
}

float GeometrySmith(
    float NdotV,
    float NdotL,
    float roughness)
{
    return
        GeometrySchlickGGX(
            NdotV,
            roughness
        ) *
        GeometrySchlickGGX(
            NdotL,
            roughness
        );
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
    float3 H =
        SafeNormalize(V + L);

    float NdotV =
        saturate(dot(N, V));

    float NdotL =
        saturate(dot(N, L));

    float NdotH =
        saturate(dot(N, H));

    float VdotH =
        saturate(dot(V, H));

    if (NdotL <= 0.0f)
        return 0.0f;

    float3 F0 =
        float3(
            0.020f,
            0.020f,
            0.020f
        );

    float3 F =
        FresnelSchlick(
            VdotH,
            F0
        );

    float D =
        DistributionGGX(
            NdotH,
            roughness
        );

    float G =
        GeometrySmith(
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

    return
        specular *
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
// Atmospheric Sky
// ============================================================

float3 GetSkyColor(
    float3 direction,
    CommonLightingParameters lighting)
{
    float height =
        saturate(direction.y);

    float3 horizon =
        float3(
            0.55f,
            0.72f,
            0.90f
        );

    float3 zenith =
        float3(
            0.08f,
            0.25f,
            0.55f
        );

    float3 sky =
        lerp(
            horizon,
            zenith,
            pow(height, 0.45f)
        );

    sky +=
        CalculateSunContribution(
            direction,
            lighting,
            256.0f
        ) * 2.0f;

    return sky;
}

#endif