#include "CommonLighting.hlsli"

cbuffer CloudConstants : register(b0)
{
    float4x4 inverseViewProjection;

    float3 cameraPosition;
    float time;

    float3 sunDirection;
    float sunStrength;

    float3 sunColor;
    float cloudDensity;

    float cloudBottom;
    float cloudTop;

    float shapeScale;
    float detailScale;

    float detailStrength;
    float absorption;

    float stepSize;
    int stepCount;

    float2 padding;
};

Texture2D gSceneColor : register(t0);
Texture2D gSceneDepth : register(t1);
SamplerState gSampler : register(s0);

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

static const float3 CLOUD_MIN =
    float3(-12000.0f, 800.0f, -12000.0f);

static const float3 CLOUD_MAX =
    float3(12000.0f, 1800.0f, 12000.0f);


// ============================================================
// Hash
// ============================================================

float Hash31(float3 p)
{
    p = frac(p * 0.1031f);

    p += dot(
        p,
        p.yzx + 33.33f
    );

    return frac(
        (p.x + p.y) * p.z
    );
}


// ============================================================
// Value Noise 3D
// ============================================================

float ValueNoise3D(float3 p)
{
    float3 cell = floor(p);
    float3 f = frac(p);

    f = f * f * (3.0f - 2.0f * f);

    float n000 = Hash31(cell);
    float n100 = Hash31(cell + float3(1, 0, 0));
    float n010 = Hash31(cell + float3(0, 1, 0));
    float n110 = Hash31(cell + float3(1, 1, 0));

    float n001 = Hash31(cell + float3(0, 0, 1));
    float n101 = Hash31(cell + float3(1, 0, 1));
    float n011 = Hash31(cell + float3(0, 1, 1));
    float n111 = Hash31(cell + float3(1, 1, 1));

    float nx00 = lerp(n000, n100, f.x);
    float nx10 = lerp(n010, n110, f.x);

    float nx01 = lerp(n001, n101, f.x);
    float nx11 = lerp(n011, n111, f.x);

    float nxy0 = lerp(nx00, nx10, f.y);
    float nxy1 = lerp(nx01, nx11, f.y);

    return lerp(nxy0, nxy1, f.z);
}


// ============================================================
// FBM
// ============================================================

float CloudFBM(float3 p)
{
    float value = 0.0f;

    value += ValueNoise3D(p) * 0.60f;

    p =
        p * 2.02f +
        float3(13.1f, 7.7f, 19.3f);

    value += ValueNoise3D(p) * 0.28f;

    p =
        p * 2.05f +
        float3(5.3f, 17.1f, 3.7f);

    value += ValueNoise3D(p) * 0.12f;

    return value;
}


// ============================================================
// Height Profile
// ============================================================

float CloudHeightProfile(float h)
{
    float bottom =
        smoothstep(
            0.0f,
            0.15f,
            h
        );

    float top =
        1.0f -
        smoothstep(
            0.72f,
            1.0f,
            h
        );

    return bottom * top;
}


// ============================================================
// Density
// ============================================================

float CloudDensityAt(
    float3 worldPosition,
    bool detail)
{
    float h =
        saturate(
            (worldPosition.y - cloudBottom) /
            max(
                cloudTop - cloudBottom,
                1.0f
            )
        );

    float heightMask =
        CloudHeightProfile(h);

    if (heightMask <= 0.001f)
        return 0.0f;


    float3 wind =
        normalize(
            float3(
                0.75f,
                0.0f,
                0.35f
            )
        );

    float3 windOffset =
        wind *
        time *
        12.0f;


    float3 shapePosition =
        (worldPosition + windOffset) *
        shapeScale;

    float shape =
        CloudFBM(shapePosition);


    float coverage =
        saturate(cloudDensity);

    float density =
        smoothstep(
            1.0f - coverage,
            1.0f,
            shape
        );


    if (detail)
    {
        float3 detailPosition =
            (worldPosition + windOffset * 1.7f) *
            detailScale;

        float detailNoise =
            ValueNoise3D(detailPosition);

        density -=
            (1.0f - detailNoise) *
            detailStrength *
            density;
    }


    density *= heightMask;

    return saturate(density);
}


// ============================================================
// Ray Box
// ============================================================

bool RayBox(
    float3 origin,
    float3 direction,
    float3 boxMin,
    float3 boxMax,
    out float tMin,
    out float tMax)
{
    float3 invDir =
        1.0f /
        max(
            abs(direction),
            1e-5f
        );

    invDir *= sign(direction);

    float3 t0 =
        (boxMin - origin) *
        invDir;

    float3 t1 =
        (boxMax - origin) *
        invDir;

    float3 tSmall =
        min(t0, t1);

    float3 tLarge =
        max(t0, t1);

    tMin =
        max(
            max(tSmall.x, tSmall.y),
            tSmall.z
        );

    tMax =
        min(
            min(tLarge.x, tLarge.y),
            tLarge.z
        );

    return
        tMax >
        max(
            tMin,
            0.0f
        );
}


// ============================================================
// World Position
// ============================================================

float3 ReconstructWorldPosition(
    float2 uv,
    float depth)
{
    float2 ndc =
        uv * 2.0f -
        1.0f;

    ndc.y =
        -ndc.y;

    float4 clip =
        float4(
            ndc,
            depth,
            1.0f
        );

    float4 world =
        mul(
            clip,
            inverseViewProjection
        );

    return
        world.xyz /
        max(
            world.w,
            1e-5f
        );
}


// ============================================================
// Phase
// ============================================================

float PhaseHG(
    float cosTheta,
    float g)
{
    float g2 = g * g;

    float denominator =
        1.0f +
        g2 -
        2.0f *
        g *
        cosTheta;

    return
        (1.0f - g2) /
        (
            4.0f *
            PI *
            pow(
                max(
                    denominator,
                    0.001f
                ),
                1.5f
            )
        );
}


// ============================================================
// Light March
// ============================================================

float SampleCloudLight(float3 position)
{
    float3 lightDirection =
        normalize(
            -sunDirection
        );

    const float LIGHT_STEP = 180.0f;

    float d0 =
        CloudDensityAt(
            position +
            lightDirection * LIGHT_STEP,
            false
        );

    float d1 =
        CloudDensityAt(
            position +
            lightDirection *
            LIGHT_STEP *
            2.0f,
            false
        );

    float opticalDepth =
        d0 * 0.75f +
        d1 * 0.50f;

    return
        exp(
            -opticalDepth *
            absorption *
            180.0f
        );
}


// ============================================================
// Pixel Shader
// ============================================================

float4 PS(PS_INPUT input) : SV_TARGET
{
    float3 sceneColor = gSceneColor.SampleLevel(gSampler, input.texcoord, 0).rgb;
    uint2 pixel = uint2(input.position.xy);
    if (((pixel.x | pixel.y) & 1u) != 0u)
        return float4(sceneColor, 1.0f);

    float3 farPosition =
        ReconstructWorldPosition(
            input.texcoord,
            1.0f
        );

    float3 rayDirection =
        normalize(
            farPosition -
            cameraPosition
        );


    float cloudEnter;
    float cloudExit;

    if (!RayBox(
        cameraPosition,
        rayDirection,
        CLOUD_MIN,
        CLOUD_MAX,
        cloudEnter,
        cloudExit))
    {
        return float4(sceneColor, 1.0f);
    }


    cloudEnter =
        max(
            cloudEnter,
            0.0f
        );


    float rayLength =
        cloudExit -
        cloudEnter;

    if (rayLength <= 0.0f)
        return float4(sceneColor, 1.0f);

    float sceneDepth = gSceneDepth.SampleLevel(gSampler, input.texcoord, 0).r;
    if (sceneDepth < 0.9999f)
    {
        float3 scenePosition = ReconstructWorldPosition(input.texcoord, sceneDepth);
        float sceneDistance = dot(scenePosition - cameraPosition, rayDirection);
        rayLength = min(rayLength, max(0.0f, sceneDistance - cloudEnter));
    }

    if (rayLength <= 0.0f)
        return float4(sceneColor, 1.0f);


    int samples =
        clamp(
            stepCount,
            4,
            12
        );


    float marchStep =
        rayLength /
        (float) samples;


    float jitter =
        Hash31(
            float3(
                input.position.xy,
                time
            )
        );


    float3 rayPosition =
        cameraPosition +
        rayDirection *
        (
            cloudEnter +
            marchStep *
            jitter *
            0.35f
        );


    float transmittance = 1.0f;

    float3 accumulatedLight =
        float3(
            0.0f,
            0.0f,
            0.0f
        );


    float3 viewDirection =
        -rayDirection;

    float3 lightDirection =
        normalize(
            -sunDirection
        );

    float cosTheta =
        dot(
            viewDirection,
            lightDirection
        );

    float phase =
        saturate(
            PhaseHG(
                cosTheta,
                0.35f
            ) * 4.0f
        );


    [loop]
    for (int i = 0; i < 12; ++i)
    {
        if (i >= samples)
            break;


        bool useDetail =
            (i == 1) ||
            (i == 3) ||
            (i == 5) ||
            (i == 7) ||
            (i == 9);


        float density =
            CloudDensityAt(
                rayPosition,
                useDetail
            );


        if (density > 0.002f)
        {
            float light =
                SampleCloudLight(
                    rayPosition
                );


            float edge =
                1.0f -
                saturate(
                    density * 3.5f
                );


            float silverLining =
                pow(
                    saturate(edge),
                    3.0f
                );


            float sunLight =
                light *
                (
                    0.55f +
                    phase * 0.45f
                );


            float3 lighting =
                sunColor *
                sunStrength *
                sunLight;


            lighting *=
                1.0f +
                silverLining * 1.25f;


            float h =
                saturate(
                    (
                        rayPosition.y -
                        cloudBottom
                    ) /
                    max(
                        cloudTop -
                        cloudBottom,
                        1.0f
                    )
                );


            lighting +=
                sunColor *
                lerp(
                    0.22f,
                    0.50f,
                    h
                );


            float opticalDepth =
                density *
                absorption *
                marchStep;


            float alpha =
                1.0f -
                exp(
                    -opticalDepth
                );


            accumulatedLight +=
                lighting *
                alpha *
                transmittance;


            transmittance *=
                1.0f -
                alpha;


            if (transmittance < 0.025f)
                break;
        }


        rayPosition +=
            rayDirection *
            marchStep;
    }


    float alpha =
        saturate(
            1.0f -
            transmittance
        );


    float3 color =
        accumulatedLight /
        max(
            alpha,
            0.001f
        );


    return float4(
        lerp(sceneColor, color, alpha),
        1.0f
    );
}