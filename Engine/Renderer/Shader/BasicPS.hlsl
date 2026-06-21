Texture2D gDiffuse : register(t0);
SamplerState gSampler : register(s0);

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float time : TEXCOORD1;
};

float4 PS(PS_INPUT input) : SV_TARGET
{
    // 屈折の動き（既存の歪み）
    float2 distortedTexcoord = input.texcoord + float2(
        sin(input.texcoord.y * 10.0f + input.time * 2.0f) * 0.01f,
        sin(input.texcoord.x * 10.0f + input.time * 2.0f) * 0.01f
    );
    
    float4 color = gDiffuse.Sample(gSampler, distortedTexcoord);

    // 簡易スペキュラ (水面のキラキラ)
    float spec = pow(max(0, sin(input.texcoord.x * 20.0f + input.time * 3.0f)), 30.0f);
    //color.rgb += spec * 0.5f; // 白いハイライトを加算

    return color;
}