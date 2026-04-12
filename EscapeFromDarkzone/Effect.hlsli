#ifndef EFFECT_HLSLI
#define EFFECT_HLSLI

#include "common.hlsli"

struct VS_PARTICLE_INPUT
{
    // 정점 데이터
    float3 position : POSITION;
    float2 uv : TEXCOORD0;
    
    // 인스턴스 데이터
    float3 instPosition : INST_POSITION;
    float instProgress : INST_PROGRESS;
    float2 instSize : INST_SIZE;
    float3 instRight : INST_RIGHT;
    float3 instUp : INST_UP;
};

struct VS_PARTICLE_OUTPUT
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float2 localUV : TEXCOORD1;
    float progress : TEXCOORD2;
};

VS_PARTICLE_OUTPUT VSParticle(VS_PARTICLE_INPUT input)
{
    VS_PARTICLE_OUTPUT output;

    float3 centerW = input.instPosition;
    float progress = input.instProgress;

    float3 vRight = normalize(input.instRight);
    float3 vUp = normalize(input.instUp);

    float3 posW = centerW
                + (input.position.x * input.instSize.x * vRight)
                + (input.position.y * input.instSize.y * vUp);

    output.position = mul(mul(float4(posW, 1), gmtxView), gmtxProjection);

    int numCols = 8;
    int numRows = 4;
    int totalFrames = numCols * numRows;

    int currentFrame = clamp((int) (progress * totalFrames), 0, totalFrames - 1);

    int col = currentFrame % numCols;
    int row = currentFrame / numCols;

    float uvWidth = 1.0f / numCols;
    float uvHeight = 1.0f / numRows;

    float cropX = 7.0f / 1456.0f;
    float cropY = 7.0f / 720.0f;

    float minU = (col * uvWidth) + cropX;
    float maxU = ((col + 1) * uvWidth) - cropX;

    float minV = (row * uvHeight) + cropY;
    float maxV = ((row + 1) * uvHeight) - cropY;

    output.uv = float2(
        lerp(minU, maxU, input.uv.x),
        lerp(minV, maxV, input.uv.y)
    );

    output.localUV = input.uv;
    output.progress = progress;

    return output;
}

float4 PSParticle(VS_PARTICLE_OUTPUT input) : SV_TARGET
{
    float4 color = gtxtAlbedoTexture.Sample(gssClamp, input.uv);

    color.rgb *= 1.2;

    if (color.a < 0.01f)
        discard;

    float dist = distance(input.localUV, float2(0.5f, 0.5f));
    float edgeFade = smoothstep(0.5f, 0.35f, dist);
    color.a *= edgeFade;
    
    color.a *= smoothstep(1.0f, 0.0f, input.progress);

    return color;
}

#endif