#ifndef C_YUY2
    #define C_YUY2 0
#endif

Texture2D tex : register(t0);
SamplerState samp : register(s0);

cbuffer PS_COLOR_TRANSFORM : register(b0)
{
    float3 cm_r;
    float3 cm_g;
    float3 cm_b;
    float3 cm_c;
    // NB: sizeof(float3) == sizeof(float4)
};
#if C_YUY2
cbuffer PS_TEX_DIMENSIONS : register(b4)
{
    float width;
    float height;
    float dx;
    float dy;
};
#endif

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float2 Tex : TEXCOORD;
};

float4 main(PS_INPUT input) : SV_Target
{
    float4 color = tex.Sample(samp, input.Tex); // original pixel
#if C_YUY2
    if (fmod(input.Tex.x*width, 2) < 1.0) {
        color = float4(color[0], color[1], color[3], 0);
    } else {
#if (C_YUY2 == 1) // nearest neighbor
        color = float4(color[2], color[1], color[3], 0);
#elif (C_YUY2 == 2) // linear
        float2 chroma0 = color.yw;
        float2 chroma1 = tex.Sample(samp, input.Tex + float2(dx, 0)).yw;
        float2 chroma = (chroma0 + chroma1) * 0.5;
        color = float4(color[2], chroma, 0);
#elif (C_YUY2 == 3) // cubic
        float2 chroma0 = tex.Sample(samp, input.Tex + float2(-dx, 0)).yw;
        float2 chroma1 = color.yw;
        float2 chroma2 = tex.Sample(samp, input.Tex + float2(dx, 0)).yw;
        float2 chroma3 = tex.Sample(samp, input.Tex + float2(2*dx, 0)).yw;
        float2 chroma = (9 * (chroma1 + chroma2) - (chroma0 + chroma3)) * 0.0625;
        color = float4(color[2], chroma, 0);
#endif
    }
#endif

    color.rgb = float3(mul(cm_r, color.rgb), mul(cm_g, color.rgb), mul(cm_b, color.rgb)) + cm_c;

    return color;
}
