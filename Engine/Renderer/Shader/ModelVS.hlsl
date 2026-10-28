cbuffer MatrixBuffer : register(b0)
{
    float4x4 mvp;
    float4x4 world;
    float time;
    float3 padding;
    float3 cameraPos;
    float pad2;
};

cbuffer PropellerBuffer : register(b1)
{
    float propellerAngle;
    float3 propellerPivot;
    float3 propellerAxis;
    uint propellerEnabled;
};

struct VS_INPUT
{
    float3 position : POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL;
    float4 tangent : TANGENT;
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 worldPos : TEXCOORD1;
    float3 normal : NORMAL;
    float4 tangent : TANGENT;
};

float3 RotatePropellerVector(float3 value)
{
    float sine = sin(propellerAngle);
    float cosine = cos(propellerAngle);
    return value * cosine + cross(propellerAxis, value) * sine +
        propellerAxis * dot(propellerAxis, value) * (1.0f - cosine);
}

PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output;
    float3 localPosition = input.position;
    float3 localNormal = input.normal;
    float3 localTangent = input.tangent.xyz;
    if (propellerEnabled != 0)
    {
        localPosition = propellerPivot + RotatePropellerVector(input.position - propellerPivot);
        localNormal = RotatePropellerVector(input.normal);
        localTangent = RotatePropellerVector(input.tangent.xyz);
    }

    output.position = mul(float4(localPosition, 1.0f), mvp);
    output.texcoord = input.texcoord;
    output.worldPos = mul(float4(localPosition, 1.0f), world).xyz;
    output.normal = normalize(mul(float4(localNormal, 0.0f), world).xyz);
    output.tangent = float4(normalize(mul(float4(localTangent, 0.0f), world).xyz), input.tangent.w);
    return output;
}
