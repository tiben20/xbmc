#ifndef C_YUY2
    #define C_YUY2 0
#endif

sampler s0 : register(s0);

float3 cm_r : register(c0);
float3 cm_g : register(c1);
float3 cm_b : register(c2);
float3 cm_c : register(c3);

#if C_YUY2
float4 p4 : register(c4);
#define width  (p4[0])
#define height (p4[1])
#define dx     (p4[2])
#define dy     (p4[3])
#endif

float4 main(float2 tex : TEXCOORD0) : COLOR
{
    float4 color = tex2D(s0, tex); // original pixel

#if C_YUY2
    if (fmod(tex.x*width, 2) < 1.0) {
        // used D3DFMT_A8R8G8B8 is used because D3DFMT_A8B8G8R8 cannot be used on some video cards
        color = float4(color[2], color[1], color[3], 0);
    } else {
#if (C_YUY2 == 1) // nearest neighbor
        color = float4(color[0], color[1], color[3], 0);
#elif (C_YUY2 == 2) // linear
        float2 chroma0 = color.yw;
        float2 chroma1 = tex2D(s0, tex + float2(dx, 0)).yw;
        float2 chroma = (chroma0 + chroma1) * 0.5;
        color = float4(color[0], chroma, 0);
#elif (C_YUY2 == 3) // cubic
        float2 chroma0 = tex2D(s0, tex + float2(-dx, 0)).yw;
        float2 chroma1 = color.yw;
        float2 chroma2 = tex2D(s0, tex + float2(dx, 0)).yw;
        float2 chroma3 = tex2D(s0, tex + float2(2*dx, 0)).yw;
        float2 chroma = (9 * (chroma1 + chroma2) - (chroma0 + chroma3)) * 0.0625;
        color = float4(color[0], chroma, 0);
#endif
    }
#endif

    color.rgb = float3(mul(cm_r, color.rgb), mul(cm_g, color.rgb), mul(cm_b, color.rgb)) + cm_c;

    return color;
}
