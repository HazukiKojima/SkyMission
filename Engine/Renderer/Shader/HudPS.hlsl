Texture2D atlas : register(t0);
SamplerState atlasSampler : register(s0);
float Hash(float2 p)
{
    return frac(sin(dot(p, float2(127.1, 311.7))) * 43758.5453);
}
float Noise(float2 p)
{
    float2 cell = floor(p), f = frac(p);
    f = f*f*(3-2*f);
    return lerp(lerp(Hash(cell), Hash(cell+float2(1,0)), f.x),
        lerp(Hash(cell+float2(0,1)), Hash(cell+1), f.x), f.y);
}
float Turbulence(float2 p)
{
    float value = 0, weight = 0.55;
    [unroll] for (int i=0; i<4; ++i) {
        value += Noise(p)*weight;
        p = mul(float2x2(1.6,-1.2,1.2,1.6),p)+7.3;
        weight *= 0.5;
    }
    return value;
}
float4 PS(float4 position : SV_POSITION, float2 uv : TEXCOORD0, float4 color : COLOR0) : SV_TARGET
{
    if (uv.x < -0.5) {
        float seed = floor(uv.y/4);
        float2 p = float2(uv.x+2, uv.y-seed*4-1);
        float2 offset = float2(seed*3.17,seed*7.31);
        float n = Turbulence(p*3.2+offset);
        float shape = 1-length(p) + (n-0.5)*0.65;
        float density = smoothstep(0,0.38,shape)*smoothstep(0.15,0.72,n);
        float light = saturate(0.65-p.y*0.25 + (n-0.5)*0.7);
        float3 tint = color.rgb*(0.5+light*0.8);
        if (color.r > 0.8) {
            float heat = saturate((n-0.25)*1.8);
            tint = lerp(float3(0.4,0.045,0.008),color.rgb,heat);
            tint = lerp(tint,float3(1,0.9,0.55),pow(heat,4)*0.65);
        }
        return float4(tint,color.a*density);
    }
    return float4(color.rgb, color.a * atlas.Sample(atlasSampler, uv).a);
}
