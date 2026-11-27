cbuffer MatrixBuffer : register(b0)
{
    float4x4 mvp;
    float4x4 world;
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
    float4 localClipPlane;
};

cbuffer NodeRotationBuffer : register(b1)
{
    float nodeRotationAngle;
    float3 nodeRotationPivot;
    float3 nodeRotationAxis;
    uint nodeRotationEnabled;
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
    float clipDistance : SV_ClipDistance0;
};

float3 RotateNodeVector(float3 value)
{
    // 指定した軸を中心にベクトルを回す
    float sine = sin(nodeRotationAngle);
    float cosine = cos(nodeRotationAngle);
    return value * cosine + cross(nodeRotationAxis, value) * sine +
           nodeRotationAxis * dot(nodeRotationAxis, value) * (1.0f - cosine);
}

PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output;
    float3 localPosition = input.position;
    float3 localNormal = input.normal;
    float3 localTangent = input.tangent.xyz;

    if (nodeRotationEnabled != 0)
    {
        // 支点まわりの回転を位置と表面方向へそろえて適用する
        localPosition = nodeRotationPivot + RotateNodeVector(input.position - nodeRotationPivot);
        localNormal = RotateNodeVector(input.normal);
        localTangent = RotateNodeVector(input.tangent.xyz);
    }

    // 回転後の位置と表面方向を描画用の座標へ変換する
    output.position = mul(float4(localPosition, 1.0f), mvp);
    output.texcoord = input.texcoord;
    output.worldPos = mul(float4(localPosition, 1.0f), world).xyz;
    output.normal = normalize(mul(float4(localNormal, 0.0f), world).xyz);
    output.tangent = float4(normalize(mul(float4(localTangent, 0.0f), world).xyz), input.tangent.w);
    output.clipDistance = dot(float4(localPosition, 1.0f), localClipPlane);
    return output;
}
