#include "CommonLighting.hlsli"

// ============================================================
// Constant Buffer
// ============================================================

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

// ============================================================
// Resources
// ============================================================

Texture2D gDiffuse : register(t0);
SamplerState gSampler : register(s0);

// ============================================================
// Input
// ============================================================

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;

    float3 worldPos : TEXCOORD1;
    float3 wavePos : TEXCOORD2;

    float3 normal : NORMAL;
    float3 tangent : TANGENT;
};

<<<<<<< Updated upstream
// �s�N�Z���V�F�[�_: �g�̃��C�e�B���O�i�f�B�t���[�Y�A�X�y�L�����A�t���l���j�Ƌ����t�H�O���������ďo��
float4 PS(PS_INPUT input) : SV_TARGET
{
    // �@���ƃJ�����E���C�g�������擾
    float3 N = normalize(input.normal);
    float3 V = normalize(cameraPos - input.worldPos);
    // The sun direction is a fixed world-space vector: surface to sun.
    float3 L = normalize(sunDirection);

    // �x�[�X�̐��F�i�[��j��@���̌X���ŕ��
    float3 deepWaterColor = float3(0.01f, 0.08f, 0.14f);
    float3 shallowWaterColor = float3(0.03f, 0.16f, 0.24f);
    float NdotL = saturate(dot(N, L));
    float3 baseColor = lerp(deepWaterColor, shallowWaterColor, NdotL * 0.25f);

    float2 normalUV = input.texcoord * 5.0f + float2(time * 0.018f, time * -0.011f);
    float3 tangent = normalize(input.tangent - N * dot(input.tangent, N));
    float3 bitangent = normalize(cross(N, tangent));
    float3 mappedNormal = gOceanNormal.Sample(gSampler, normalUV).xyz * 2.0f - 1.0f;
    mappedNormal.xy *= 0.55f;
    N = normalize(tangent * mappedNormal.x + bitangent * mappedNormal.y + N * mappedNormal.z);
    NdotL = saturate(dot(N, L));
    baseColor = lerp(deepWaterColor, shallowWaterColor, NdotL * 0.25f);

    // �e�N�X�`�����y��������i�@���ɂ��c�� + ���Ԃł킸���ɓ������j
    float2 waveScroll = float2(time * 0.012f, time * -0.008f);
    float2 waterUV = input.texcoord * 8.0f;
    float3 texA = gDiffuse.Sample(gSampler, waterUV + N.xz * 0.035f + waveScroll).rgb;
    float3 texB = gDiffuse.Sample(gSampler, waterUV * 0.63f - N.zx * 0.02f - waveScroll * 0.7f).rgb;
    float3 tex = lerp(texA, texB, 0.35f);

    // World-space sun glint. It is driven by the surface normal and sun direction,
    // so its position does not move with the camera.
    float3 waterF0 = float3(0.02f, 0.02f, 0.02f);
    float sunGlint = pow(NdotL, 96.0f) * 0.18f;
    float3 spec = sunColor * sunIntensity * sunGlint;
    float3 directDiffuse = baseColor * (1.0f - waterF0) * NdotL * sunIntensity;

    // �t���l���i���p�ˑ��̔��ˁj
    // ��F�i�t���l���Ƌ����ɍ����邽�߂̐F�j
    float3 skyColor = float3(0.45f, 0.68f, 0.9f);

    // ����: �x�[�X + �e�N�X�`���̔�����Z + �X�y�L�����A�t���l���ŋ�𔽎�
    float3 ambient = ambientColor * ambientIntensity * lerp(0.65f, 1.0f, saturate(N.y));
    float3 color = ambient + directDiffuse + baseColor * 0.08f + tex * 0.12f + spec;
    float worldSkyLight = smoothstep(0.0f, 1.0f, saturate(N.y)) * 0.08f;
    color += skyColor * worldSkyLight;
=======
// ============================================================
// Main
// ============================================================

float4 PS(PS_INPUT input) : SV_TARGET
{
    // --------------------------------------------------------
    // View / Light Direction
    // --------------------------------------------------------

    float3 V = SafeNormalize(cameraPos - input.worldPos);
    float3 L = SafeNormalize(sunDirection);

    // --------------------------------------------------------
    // Normal
    // --------------------------------------------------------

    float3 N;
    float3 T;
    float3 B;

    BuildTBN(input.normal, input.tangent, N, T, B);

    float NdotV = saturate(dot(N, V));
    float NdotL = saturate(dot(N, L));

    // --------------------------------------------------------
    // Diffuse
    // --------------------------------------------------------
>>>>>>> Stashed changes

    float3 albedo = gDiffuse.Sample(gSampler, input.texcoord).rgb;
    float3 diffuse = albedo * NdotL;

    // --------------------------------------------------------
    // Ambient
    // --------------------------------------------------------

    float3 ambient = albedo * ambientColor * ambientIntensity;

    // --------------------------------------------------------
    // Sun Specular
    // --------------------------------------------------------

    float3 sunSpecular = CalculateSunSpecular(
        N,
        V,
        L,
        0.35f,
        sunColor,
        sunIntensity
    );

    // --------------------------------------------------------
    // Final Lighting
    // --------------------------------------------------------

    float3 color = diffuse + ambient + sunSpecular;

    // --------------------------------------------------------
    // Atmospheric Fog
    // --------------------------------------------------------

<<<<<<< Updated upstream
    // �����t�H�O�i���i�̔g�����������Ȃ��悤�ɊJ�n���������߂ɐݒ�j
    float distanceToCamera = length(cameraPos - input.worldPos);
    const float fogStart = 600.0f;
    const float fogEnd = 3000.0f;
    float fogFactor = saturate((distanceToCamera - fogStart) / (fogEnd - fogStart));
    color = lerp(color, skyColor, fogFactor);

    // Map HDR lighting into the displayable range without clipping highlights.
=======
    float distanceToCamera = length(cameraPos - input.worldPos);

    float fogStart = 1500.0f;
    float fogEnd = 12000.0f;

    float fogFactor = saturate(
        (distanceToCamera - fogStart) /
        (fogEnd - fogStart)
    );

    float3 atmosphere = GetSkyColor(
        V,
        sunDirection,
        sunColor,
        sunIntensity
    );

    color = lerp(color, atmosphere, fogFactor);

    // --------------------------------------------------------
    // Tonemapping
    // --------------------------------------------------------

>>>>>>> Stashed changes
    color = color / (1.0f + color);

    return float4(saturate(color), 1.0f);
}
