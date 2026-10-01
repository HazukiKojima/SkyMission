Texture2D gSkyTexture : register(t0);
SamplerState gSampler : register(s0);

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 worldPos : TEXCOORD1;
};

<<<<<<< Updated upstream
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
=======
float4 main(PS_INPUT input) : SV_TARGET
{
    // 1. テクスチャのサンプリング
    float3 rawColor = gSkyTexture.Sample(gSampler, input.texcoord).rgb;
    
    // 2. 露出とベースの「明るい空色」のブレンド（暗部の底上げを強力に適用）
    float exposure = 1.0f;
    float3 color = rawColor * exposure;
    
    // 太陽と反対側（暗い部分）が暗く沈むのを防ぐため、明るい青で強力に底上げ
    color = max(color, float3(0.22f, 0.40f, 0.65f));

    // 3. トーンマッピング
    color = color / (1.0f + color);
    
    return float4(max(color, 0.0f), 1.0f);
}
>>>>>>> Stashed changes
