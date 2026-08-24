Texture2D gSkyTexture : register(t0);
SamplerState gSampler : register(s0);

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 worldPos : TEXCOORD1;
};

// Sky Sphere Pixel Shader
// HDR/EXR�e�N�X�`����T���v�����O���ďo��
float4 main(PS_INPUT input) : SV_TARGET
{
    // UV���W����V��e�N�X�`����T���v�����O
    float3 color = gSkyTexture.Sample(gSampler, input.texcoord).rgb;
    
    // ���j�A�J���[��ԂŕԂ��i�t���[���o�b�t�@��HDR�Ή��̏ꍇ�j
    color = color / (1.0f + color);
        return float4(max(color, 0.0f), 1.0f);
}
