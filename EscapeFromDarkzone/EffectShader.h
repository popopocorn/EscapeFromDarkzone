//EffectShader.h
#pragma once
#include "Shader.h"


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

    /*virtual void CreateShadowShader(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, ID3D12RootSignature* pd3dGraphicsRootSignature) override
    {
        m_pd3dPipelineState.push_back(nullptr);
    }*/

protected:
    int m_nRootParameterStartIndex = 0;
public:
    float m_fAge = 0.0f;
    float m_fLifeTime = 1.0f;
};