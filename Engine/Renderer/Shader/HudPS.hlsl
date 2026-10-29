Texture2D atlas : register(t0);
SamplerState atlasSampler : register(s0);
float4 PS(float4 position : SV_POSITION, float2 uv : TEXCOORD0, float4 color : COLOR0) : SV_TARGET
{
    return float4(color.rgb, color.a * atlas.Sample(atlasSampler, uv).a);
}
