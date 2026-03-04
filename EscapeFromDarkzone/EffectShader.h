#pragma once
#include "Shader.h"

struct EFFECT_INFO
{
    float fAge;
    float fLifeTime;
    float fProgress;
    float padding;
};

class CEffectShader : public CShader
{
public:
    CEffectShader();
    virtual ~CEffectShader();

    virtual D3D12_INPUT_LAYOUT_DESC CreateInputLayout() override;
    virtual D3D12_BLEND_DESC CreateBlendState() override;
    virtual D3D12_DEPTH_STENCIL_DESC CreateDepthStencilState() override;

    virtual D3D12_SHADER_BYTECODE CreateVertexShader(ID3DBlob** ppd3dShaderBlob);
    virtual D3D12_SHADER_BYTECODE CreateGeometryShader(ID3DBlob** ppd3dShaderBlob);
    virtual D3D12_SHADER_BYTECODE CreatePixelShader(ID3DBlob** ppd3dShaderBlob);

    virtual void CreateGraphicsPipelineState(ID3D12Device* pd3dDevice, ID3D12RootSignature* pd3dGraphicsRootSignature, int nRootParameterStartIndex);

    virtual void CreateShaderVariables(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList) override;
    virtual void UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList) override;
    virtual void ReleaseShaderVariables() override;

protected:
    int m_nRootParameterStartIndex = 0;

    ID3D12Resource* m_pd3dcbEffectInfo = NULL;
    EFFECT_INFO* m_pcbMappedEffectInfo = NULL;

public:
    float m_fAge = 0.0f;
    float m_fLifeTime = 1.0f;
};