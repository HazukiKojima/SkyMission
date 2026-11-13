struct Output
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};
Output VS(float2 position : POSITION, float2 uv : TEXCOORD0, float4 color : COLOR0)
{
    Output result;
    result.position = float4(position, 0, 1);
    result.uv = uv;
    result.color = color;
    return result;
}
