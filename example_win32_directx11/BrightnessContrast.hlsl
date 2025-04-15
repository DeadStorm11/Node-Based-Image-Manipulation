// BrightnessContrast.hlsl

Texture2D inputTexture : register(t0);
SamplerState inputSampler : register(s0);

cbuffer BrightnessContrastBuffer : register(b0)
{
    float brightness;
    float contrast;
}

float4 main(float2 texCoord : TEXCOORD) : SV_Target
{
    float4 color = inputTexture.Sample(inputSampler, texCoord);

    // Apply contrast
    color.rgb = (color.rgb - 0.5f) * contrast + 0.5f;

    // Apply brightness
    color.rgb += brightness;

    return saturate(color); // Clamp between 0 and 1
}
