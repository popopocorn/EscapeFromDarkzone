// EffectShader.cpp
#include "stdafx.h"
#include "EffectShader.h"

CEffectShader::CEffectShader() {}
CEffectShader::~CEffectShader() {}

D3D12_INPUT_LAYOUT_DESC CEffectShader::CreateInputLayout()
{
    UINT nInputElementDescs = 2;
    D3D12_INPUT_ELEMENT_DESC* pDescs = new D3D12_INPUT_ELEMENT_DESC[nInputElementDescs];

    pDescs[0] = { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };

    pDescs[1] = { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };

    D3D12_INPUT_LAYOUT_DESC layout;
    layout.pInputElementDescs = pDescs;
    layout.NumElements = nInputElementDescs;
    return layout;
}

D3D12_BLEND_DESC CEffectShader::CreateBlendState()
{
    D3D12_BLEND_DESC desc = CShader::CreateBlendState();
    desc.RenderTarget[0].BlendEnable = TRUE;
    desc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    desc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE; // Additive Blending
    desc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    return desc;
}

D3D12_DEPTH_STENCIL_DESC CEffectShader::CreateDepthStencilState()
{
    D3D12_DEPTH_STENCIL_DESC desc = CShader::CreateDepthStencilState();
    desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    return desc;
}

D3D12_RASTERIZER_DESC CEffectShader::CreateRasterizerState()
{
    D3D12_RASTERIZER_DESC desc = CShader::CreateRasterizerState();

    desc.CullMode = D3D12_CULL_MODE_NONE;

    return desc;
}

D3D12_SHADER_BYTECODE CEffectShader::CreateVertexShader(ID3DBlob** ppd3dShaderBlob)
{
    return CShader::CompileShaderFromFile(L"Effect.hlsli", "VSParticle", "vs_5_1", ppd3dShaderBlob);
}

D3D12_SHADER_BYTECODE CEffectShader::CreatePixelShader(ID3DBlob** ppd3dShaderBlob)
{
    return CShader::CompileShaderFromFile(L"Effect.hlsli", "PSParticle", "ps_5_1", ppd3dShaderBlob);
}

void CEffectShader::CreateGraphicsPipelineState(ID3D12Device* device, ID3D12RootSignature* rootSig, int rootParamStart)
{
    m_nRootParameterStartIndex = rootParamStart;

    ID3DBlob* vsBlob = NULL;
    ID3DBlob* psBlob = NULL;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc;
    ::ZeroMemory(&desc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

    desc.BlendState = CreateBlendState();
    desc.RasterizerState = CreateRasterizerState();
    desc.DepthStencilState = CreateDepthStencilState();
    desc.pRootSignature = rootSig;

    desc.VS = CreateVertexShader(&vsBlob);
    desc.PS = CreatePixelShader(&psBlob);
    desc.GS = { nullptr, 0 };

    desc.InputLayout = CreateInputLayout();

    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    desc.SampleDesc.Count = 1;
    desc.SampleMask = UINT_MAX;

    ID3D12PipelineState* pPipelineState = NULL;
    HRESULT hr = device->CreateGraphicsPipelineState(&desc, __uuidof(ID3D12PipelineState), (void**)&pPipelineState);

    if (FAILED(hr))
    {
        char buf[256];
        sprintf_s(buf, "Effect PSO 생성 실패: 0x%08X\n", hr);
        OutputDebugStringA(buf);
        __debugbreak();
    }
    else
    {
        if (m_pd3dPipelineState.empty()) m_pd3dPipelineState.push_back(pPipelineState);
        else m_pd3dPipelineState[0] = pPipelineState;
    }

    if (vsBlob) vsBlob->Release();
    if (psBlob) psBlob->Release();
    if (desc.InputLayout.pInputElementDescs) delete[] desc.InputLayout.pInputElementDescs;
}