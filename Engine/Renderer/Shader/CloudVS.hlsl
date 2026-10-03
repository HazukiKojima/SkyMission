struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

PS_INPUT VS(uint vertexID : SV_VertexID)
{
    PS_INPUT output;

    float2 position;
    float2 uv;

    if (vertexID == 0)
    {
        position = float2(-1.0f, -1.0f);
        uv = float2(0.0f, 1.0f);
    }
    else if (vertexID == 1)
    {
        position = float2(-1.0f, 3.0f);
        uv = float2(0.0f, -1.0f);
    }
    else
    {
        position = float2(3.0f, -1.0f);
        uv = float2(2.0f, 1.0f);
    }

    output.position = float4(position, 0.0f, 1.0f);
    output.texcoord = uv;

    return output;
}