#include "common.hlsli"

cbuffer cbEffectInfo : register(b10)
{
    float g_fAge; // 현재 나이
    float g_fLifeTime; // 총 수명
    float g_fProgress; // 진행도 (0.0 ~ 1.0)
    float padding; // 16바이트 정렬
};

//Vertex Shader 
struct VS_EFFECT_INPUT
{
    float3 position : POSITION;
    float2 size : TEXCOORD0;
};

struct VS_EFFECT_OUTPUT
{
    float3 positionW : POSITION;
    float2 size : SIZE;
};

VS_EFFECT_OUTPUT VSEffect(VS_EFFECT_INPUT input)
{
    VS_EFFECT_OUTPUT output;
    
    output.positionW = mul(float4(input.position, 1.0f), gmtxGameObject).xyz;
    output.size = input.size;
    
    return output;
}

//Geometry Shader
struct GS_EFFECT_OUTPUT
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

[maxvertexcount(4)]
void GSEffect(point VS_EFFECT_OUTPUT input[1], inout TriangleStream<GS_EFFECT_OUTPUT> outStream)
{
    GS_EFFECT_OUTPUT output;
    
    //billboard
    float3 vLook = normalize(gvCameraPosition - input[0].positionW);
    float3 vUp = float3(0.0f, 1.0f, 0.0f);
    float3 vRight = normalize(cross(vUp, vLook));
    vUp = cross(vLook, vRight);

    float halfWidth = input[0].size.x * 0.5f;
    float halfHeight = input[0].size.y * 0.5f;

    float4 v[4];
    v[0] = float4(input[0].positionW + halfWidth * vRight - halfHeight * vUp, 1.0f); // Bottom-Right
    v[1] = float4(input[0].positionW + halfWidth * vRight + halfHeight * vUp, 1.0f); // Top-Right
    v[2] = float4(input[0].positionW - halfWidth * vRight - halfHeight * vUp, 1.0f); // Bottom-Left
    v[3] = float4(input[0].positionW - halfWidth * vRight + halfHeight * vUp, 1.0f); // Top-Left

    float2 texCoord[4] = { float2(1.0f, 1.0f), float2(1.0f, 0.0f), float2(0.0f, 1.0f), float2(0.0f, 0.0f) };

    matrix mtxViewProj = mul(gmtxView, gmtxProjection);

    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        output.position = mul(v[i], mtxViewProj);
        output.uv = texCoord[i];
        outStream.Append(output);
    }
}

//Pixel Shader
float4 PSEffect(GS_EFFECT_OUTPUT input) : SV_TARGET
{
    float4 cColor = gtxtAlbedoTexture.Sample(gssWrap, input.uv);
    
    cColor.a *= (1.0f - g_fProgress);

    clip(cColor.a - 0.05f);

    return cColor;
}