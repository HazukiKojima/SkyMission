struct FlameBuffer
{
    float4x4 mvp;
    float time;
    float3 padding;
};

cbuffer FlameParameters : register(b0)
{
    float4x4 flameMvp;
    float flameTime;
    float3 flamePadding;
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

float4 PS(PS_INPUT input) : SV_TARGET
{
    float lengthFactor = saturate(input.texcoord.y);
    float centerFactor = 1.0f - saturate(abs(input.texcoord.x - 0.5f) * 2.0f);
    float core = pow(centerFactor, 0.65f) * (1.0f - lengthFactor * 0.55f);
    float3 color = lerp(float3(1.0f, 0.16f, 0.01f), float3(1.0f, 0.95f, 0.55f), core);
    float alpha = (1.0f - lengthFactor * 0.72f) * (0.35f + core * 0.65f);
    return float4(color, alpha);
}
