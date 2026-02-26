#pragma once


#pragma once
class ShadowMap{
private:
    ID3D12Resource*                                             ShadowMapResource = NULL;
    ID3D12DescriptorHeap*                                       DsvHeap = NULL;          
    D3D12_GPU_DESCRIPTOR_HANDLE                                 SrvGpuHandle = {};
    D3D12_RESOURCE_STATES                                       ResourceState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    std::array<D3D12_CPU_DESCRIPTOR_HANDLE, CASCADE_COUNT>      DsvHandles;
public:
    void Create(ID3D12Device* pd3dDevice);
    void CreateSRV(ID3D12Device* pd3dDevice,
        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
        D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle);
    void Release();

    // Shadow 패스 - i번째 슬라이스 DSV 바인딩
    void BindAsDepthTarget(ID3D12GraphicsCommandList* pd3dCommandList, int cascadeIndex);

    // 리소스 상태 전환
    void TransitionToSRV(ID3D12GraphicsCommandList* pd3dCommandList);
    void TransitionToDSV(ID3D12GraphicsCommandList* pd3dCommandList);

    // 씬 힙에 SRV 등록 후 핸들 저장
    void SetSrvGpuHandle(D3D12_GPU_DESCRIPTOR_HANDLE handle) { SrvGpuHandle = handle; }
    D3D12_GPU_DESCRIPTOR_HANDLE GetSrvGpuHandle() const { return SrvGpuHandle; }

    ID3D12Resource* GetResource() const { return ShadowMapResource; }
};