#ifndef EFFECT_HLSLI
#define EFFECT_HLSLI

#include "common.hlsli"

cbuffer cbEffectInfo : register(b10)
{
    float g_fAge;
    float g_fLifeTime;
    float g_fProgress;
    float padding;
};

struct VS_PARTICLE_INPUT
{
    float3 position : POSITION;
    float2 uv : TEXCOORD0;
};

struct VS_PARTICLE_OUTPUT
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VS_PARTICLE_OUTPUT VSParticle(VS_PARTICLE_INPUT input)
{
    VS_PARTICLE_OUTPUT output;

    float3 centerW = float3(gmtxGameObject._41, gmtxGameObject._42, gmtxGameObject._43);

    float3 vLook = normalize(gvCameraPosition - centerW);
    float3 vUp = float3(0.0f, 1.0f, 0.0f);
    
    float3 vRight = normalize(cross(vLook, vUp));
    vUp = normalize(cross(vRight, vLook));

    float3 posW = centerW + (input.position.x * vRight) + (input.position.y * vUp);
    
    output.position = mul(mul(float4(posW, 1.0f), gmtxView), gmtxProjection);

    int numCols = 8;
    int numRows = 8;
    int totalFrames = numCols * numRows;
    
    int currentFrame = clamp((int) (g_fProgress * totalFrames), 0, totalFrames - 1);
    int col = currentFrame % numCols;
    int row = currentFrame / numCols;

    float uvWidth = 1.0f / numCols;
    float uvHeight = 1.0f / numRows;
    float epsilon = 0.002f;

    float2 finalUV = float2((col + input.uv.x) * uvWidth, (row + input.uv.y) * uvHeight);
    
    if (input.uv.x > 0.5f)
        finalUV.x -= epsilon;
    else
        finalUV.x += epsilon;
    if (input.uv.y > 0.5f)
        finalUV.y -= epsilon;
    else
        finalUV.y += epsilon;

    output.uv = finalUV;

    return output;
}

float4 PSParticle(VS_PARTICLE_OUTPUT input) : SV_TARGET
{
    float4 color = gtxtAlbedoTexture.Sample(gssWrap, input.uv);
    
    color.a *= (1.0f - g_fProgress);
    
    return color;
}
#endif