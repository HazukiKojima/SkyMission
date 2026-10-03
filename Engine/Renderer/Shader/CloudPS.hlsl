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

SamplerState gSampler : register(s0);
Texture2D gSceneColor : register(t0);
Texture2D gSceneDepth : register(t1);

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

static const float3 CLOUD_MIN = float3(-12000.0f, 800.0f, -12000.0f);
static const float3 CLOUD_MAX = float3( 12000.0f, 1800.0f,  12000.0f);

float Hash31(float3 p)
{
    p = frac(p * 0.1031f);
    p += dot(p, p.yzx + 33.33f);
    return frac((p.x + p.y) * p.z);
}

float ValueNoise3D(float3 p)
{
    float3 cell = floor(p);
    float3 local = frac(p);
    local = local * local * (3.0f - 2.0f * local);

    float n000 = Hash31(cell + float3(0, 0, 0));
    float n100 = Hash31(cell + float3(1, 0, 0));
    float n010 = Hash31(cell + float3(0, 1, 0));
    float n110 = Hash31(cell + float3(1, 1, 0));
    float n001 = Hash31(cell + float3(0, 0, 1));
    float n101 = Hash31(cell + float3(1, 0, 1));
    float n011 = Hash31(cell + float3(0, 1, 1));
    float n111 = Hash31(cell + float3(1, 1, 1));

    float nx00 = lerp(n000, n100, local.x);
    float nx10 = lerp(n010, n110, local.x);
    float nx01 = lerp(n001, n101, local.x);
    float nx11 = lerp(n011, n111, local.x);
    return lerp(lerp(nx00, nx10, local.y), lerp(nx01, nx11, local.y), local.z);
}

float FbmShape(float3 p)
{
    float value = 0.0f;
    float amplitude = 0.5f;
    float frequency = 1.0f;
    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        value += ValueNoise3D(p * frequency) * amplitude;
        frequency *= 2.03f;
        amplitude *= 0.5f;
    }
    return value;
}

float WorleyLike(float3 p)
{
    float3 cell = floor(p);
    float3 local = frac(p) - 0.5f;
    float nearest = 1.0f;
    [unroll]
    for (int z = -1; z <= 1; ++z)
    {
        for (int y = -1; y <= 1; ++y)
        {
            for (int x = -1; x <= 1; ++x)
            {
                float3 offset = float3(x, y, z);
                float3 featurePoint = Hash31(cell + offset).xxx;
                featurePoint = frac(featurePoint * float3(1.73f, 2.41f, 3.17f)) - 0.5f;
                nearest = min(nearest, length(offset + featurePoint - local));
            }
        }
    }
    return 1.0f - saturate(nearest * 1.35f);
}

float CloudDensity(float3 worldPos)
{
    float height01 = saturate((worldPos.y - cloudBottom) / max(cloudTop - cloudBottom, 1.0f));
    float bottomGradient = smoothstep(0.02f, 0.22f, height01);
    float topGradient = 1.0f - smoothstep(0.72f, 0.98f, height01);
    float heightGradient = bottomGradient * topGradient;

    float3 windOffset = float3(time * 0.012f, time * 0.002f, -time * 0.008f);
    float3 shapeCoord = worldPos * shapeScale + windOffset;
    float shape = FbmShape(shapeCoord);
    float detail = ValueNoise3D(worldPos * detailScale + windOffset * 2.0f);
    float billow = WorleyLike(worldPos * shapeScale * 2.2f + windOffset * 0.7f);

    float coverage = shape * 0.72f + billow * 0.28f;
    coverage += (detail - 0.5f) * detailStrength;
    float density = smoothstep(0.56f, 0.72f, coverage) * heightGradient;
    return saturate(density * cloudDensity);
}

bool RayBox(float3 origin, float3 direction, out float tEnter, out float tExit)
{
    float3 inverseDirection = 1.0f / direction;
    float3 t0 = (CLOUD_MIN - origin) * inverseDirection;
    float3 t1 = (CLOUD_MAX - origin) * inverseDirection;
    float3 nearPoint = min(t0, t1);
    float3 farPoint = max(t0, t1);

    tEnter = max(max(nearPoint.x, nearPoint.y), nearPoint.z);
    tExit = min(min(farPoint.x, farPoint.y), farPoint.z);
    return tExit >= max(tEnter, 0.0f);
}

float3 ReconstructWorld(float2 uv, float depth)
{
    float2 ndc = uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f);
    float4 world = mul(float4(ndc, depth, 1.0f), inverseViewProjection);
    return world.xyz / max(world.w, 1e-5f);
}

float SampleLightTransmittance(float3 position)
{
    float3 lightDirection = normalize(sunDirection);
    float lightDistance = 260.0f;
    float lightStep = lightDistance / 6.0f;
    float opticalDepth = 0.0f;

    [unroll]
    for (int i = 0; i < 6; ++i)
    {
        position += lightDirection * lightStep;
        opticalDepth += CloudDensity(position) * lightStep;
    }

    return exp(-opticalDepth * absorption);
}

float4 PS(PS_INPUT input) : SV_TARGET
{
    float3 sceneColor = gSceneColor.SampleLevel(gSampler, input.texcoord, 0).rgb;
    float3 farWorld = ReconstructWorld(input.texcoord, 1.0f);
    float3 rayDirection = normalize(farWorld - cameraPosition);

    float tEnter;
    float tExit;
    if (!RayBox(cameraPosition, rayDirection, tEnter, tExit))
        return float4(sceneColor, 1.0f);

    tEnter = max(tEnter, 0.0f);
    if (tExit <= tEnter)
        return float4(sceneColor, 1.0f);

    float marchLength = tExit - tEnter;
    float sceneDepth = gSceneDepth.SampleLevel(gSampler, input.texcoord, 0).r;
    if (sceneDepth < 0.9999f)
    {
        float3 sceneWorld = ReconstructWorld(input.texcoord, sceneDepth);
        float sceneDistance = max(0.0f, dot(sceneWorld - cameraPosition, rayDirection));
        marchLength = min(marchLength, max(0.0f, sceneDistance - tEnter));
    }
    if (marchLength <= 0.0f)
        return float4(sceneColor, 1.0f);
    int steps = min(max(stepCount, 1), 96);
    float marchStep = max(stepSize, marchLength / (float)steps);
    marchStep = min(marchStep, 80.0f);
    float3 position = cameraPosition + rayDirection * tEnter;
    float transmittance = 1.0f;
    float3 cloudLight = 0.0f;
    float marched = 0.0f;

    [loop]
    for (int i = 0; i < 128; ++i)
    {
        if (i >= steps || marched >= marchLength || transmittance < 0.02f)
            break;

        float density = CloudDensity(position);
        if (density > 0.001f)
        {
            float lightTransmittance = SampleLightTransmittance(position);
            float3 viewDirection = -rayDirection;
            float sunDot = saturate(dot(viewDirection, normalize(sunDirection)));
            float forwardScatter = pow(sunDot, 6.0f) * 0.65f;
            float rim = pow(1.0f - saturate(density * 2.5f), 2.0f) * 0.35f;
            float3 light = sunColor * (0.45f + sunStrength * lightTransmittance);
            light += sunColor * (forwardScatter + rim);
            light += float3(0.18f, 0.24f, 0.32f);

            float opticalDepth = density * marchStep * absorption;
            float stepTransmittance = exp(-opticalDepth);
            cloudLight += light * density * marchStep * transmittance * 0.45f;
            transmittance *= stepTransmittance;
        }

        position += rayDirection * marchStep;
        marched += marchStep;
    }

    float cloudAlpha = saturate(1.0f - transmittance) * 0.72f;
    float3 color = lerp(sceneColor, saturate(cloudLight), cloudAlpha);
    return float4(color, 1.0f);
}
