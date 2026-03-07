//Effect.hlsli
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

// Geometry Shader
struct GS_EFFECT_OUTPUT
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

[maxvertexcount(4)]
void GSEffect(point VS_EFFECT_OUTPUT input[1], inout TriangleStream<GS_EFFECT_OUTPUT> outStream)
{
    GS_EFFECT_OUTPUT output;
    
    float3 vLook = normalize(gvCameraPosition - input[0].positionW);
    float3 vUp = float3(0.0f, 1.0f, 0.0f);
    float3 vRight = normalize(cross(vUp, vLook));
    vUp = cross(vLook, vRight);

    float halfWidth = input[0].size.x * 0.5f;
    float halfHeight = input[0].size.y * 0.5f;

    float4 v[4];
    v[0] = float4(input[0].positionW + halfWidth * vRight - halfHeight * vUp, 1.0f);
    v[1] = float4(input[0].positionW + halfWidth * vRight + halfHeight * vUp, 1.0f);
    v[2] = float4(input[0].positionW - halfWidth * vRight - halfHeight * vUp, 1.0f);
    v[3] = float4(input[0].positionW - halfWidth * vRight + halfHeight * vUp, 1.0f);

    int numCols = 8;
    int numRows = 4;
    int totalFrames = numCols * numRows;

    int currentFrame = (int) (g_fProgress * (totalFrames - 1));

    int col = currentFrame % numCols;
    int row = currentFrame / numCols;

    float uvWidth = 1.0f / numCols;
    float uvHeight = 1.0f / numRows;

    float startU = col * uvWidth;
    float startV = row * uvHeight;
    
    float epsilon = 0.002f;
    
    float2 texCoord[4] =
    {
        float2(startU + uvWidth - epsilon, startV + uvHeight - epsilon),
        float2(startU + uvWidth - epsilon, startV + epsilon),
        float2(startU + epsilon, startV + uvHeight - epsilon),
        float2(startU + epsilon, startV + epsilon)
    };

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
    
    float brightness = max(cColor.r, max(cColor.g, cColor.b));
    cColor.a = brightness;

    cColor.a *= (1.0f - g_fProgress);

    return cColor;
}