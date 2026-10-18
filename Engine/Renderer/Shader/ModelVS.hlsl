cbuffer MatrixBuffer : register(b0)
{
    float4x4 mvp;
    float4x4 world;
    float time;
    float3 padding;
    float3 cameraPos;
    float pad2;
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

PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output;
    output.position = mul(float4(input.position, 1.0f), mvp);
    output.texcoord = input.texcoord;
    output.worldPos = mul(float4(input.position, 1.0f), world).xyz;
    output.normal = normalize(mul(float4(input.normal, 0.0f), world).xyz);
    output.tangent = float4(normalize(mul(float4(input.tangent.xyz, 0.0f), world).xyz), input.tangent.w);
    return output;
}
