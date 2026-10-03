struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

VS_OUTPUT VS(uint vertexId : SV_VertexID)
{
    VS_OUTPUT output;
    float2 positions[3] = {
        float2(-1.0f, -1.0f),
        float2(-1.0f,  3.0f),
        float2( 3.0f, -1.0f)
    };

    output.position = float4(positions[vertexId], 0.0f, 1.0f);
    output.texcoord = output.position.xy * float2(0.5f, -0.5f) + 0.5f;
    return output;
}
