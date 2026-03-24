#include "stdafx.h"
#include "ShadowMap.h"

void ShadowMap::Create(ID3D12Device* pd3dDevice)
{
	//쉐도우맵 리소스 생성
	D3D12_RESOURCE_DESC d3dResourceDesc;
	::ZeroMemory(&d3dResourceDesc, sizeof(D3D12_RESOURCE_DESC));
	d3dResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	d3dResourceDesc.Alignment = 0;
	d3dResourceDesc.Width = SHADOW_MAP_SIZE;
	d3dResourceDesc.Height = SHADOW_MAP_SIZE;
	d3dResourceDesc.DepthOrArraySize = CASCADE_COUNT;
	d3dResourceDesc.MipLevels = 1;
	d3dResourceDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	d3dResourceDesc.SampleDesc.Count = 1;
	d3dResourceDesc.SampleDesc.Quality = 0;
	d3dResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	d3dResourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_HEAP_PROPERTIES d3dHeapProperties;
	::ZeroMemory(&d3dHeapProperties, sizeof(D3D12_HEAP_PROPERTIES));
	d3dHeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	d3dHeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	d3dHeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	d3dHeapProperties.CreationNodeMask = 1;
	d3dHeapProperties.VisibleNodeMask = 1;

	D3D12_CLEAR_VALUE d3dClearValue;
	d3dClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	d3dClearValue.DepthStencil.Depth = 1.0f;
	d3dClearValue.DepthStencil.Stencil = 0;

	HRESULT hResult = pd3dDevice->CreateCommittedResource(
		&d3dHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&d3dResourceDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&d3dClearValue,
		__uuidof(ID3D12Resource),
		(void**)&ShadowMapResource
	);

	//dsv힙 생성
	D3D12_DESCRIPTOR_HEAP_DESC d3dDescriptorHeapDesc;
	::ZeroMemory(&d3dDescriptorHeapDesc, sizeof(D3D12_DESCRIPTOR_HEAP_DESC));
	d3dDescriptorHeapDesc.NumDescriptors = CASCADE_COUNT;
	d3dDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	d3dDescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	d3dDescriptorHeapDesc.NodeMask = 0;

	hResult = pd3dDevice->CreateDescriptorHeap(
		&d3dDescriptorHeapDesc,
		__uuidof(ID3D12DescriptorHeap),
		(void**)&DsvHeap
	);

	D3D12_DEPTH_STENCIL_VIEW_DESC d3dShadowDsvDesc;
	::ZeroMemory(&d3dShadowDsvDesc, sizeof(D3D12_DEPTH_STENCIL_VIEW_DESC));
	d3dShadowDsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	d3dShadowDsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
	d3dShadowDsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	d3dShadowDsvDesc.Texture2DArray.MipSlice = 0;  
	d3dShadowDsvDesc.Texture2DArray.ArraySize = 1;  

	UINT dsvIncrementSize = pd3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	D3D12_CPU_DESCRIPTOR_HANDLE handle = DsvHeap->GetCPUDescriptorHandleForHeapStart();

	for (int i = 0; i < CASCADE_COUNT; i++)
	{
		d3dShadowDsvDesc.Texture2DArray.FirstArraySlice = i; 
		pd3dDevice->CreateDepthStencilView(ShadowMapResource, &d3dShadowDsvDesc, handle);
		DsvHandles[i] = handle;
		handle.ptr += dsvIncrementSize; 
	}
}


void ShadowMap::CreateSRV(ID3D12Device* pd3dDevice,
	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
	::ZeroMemory(&srvDesc, sizeof(D3D12_SHADER_RESOURCE_VIEW_DESC));
	srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2DArray.MostDetailedMip = 0;
	srvDesc.Texture2DArray.MipLevels = 1;
	srvDesc.Texture2DArray.FirstArraySlice = 0;
	srvDesc.Texture2DArray.ArraySize = CASCADE_COUNT;

	pd3dDevice->CreateShaderResourceView(ShadowMapResource, &srvDesc, cpuHandle);

	SrvGpuHandle = gpuHandle;
}
void ShadowMap::Release()
{
	if (ShadowMapResource)
	{
		ShadowMapResource->Release();
		ShadowMapResource = NULL;
	}
	if (DsvHeap)
	{
		DsvHeap->Release();
		DsvHeap = NULL;
	}
}

void ShadowMap::BindAsDepthTarget(ID3D12GraphicsCommandList* pd3dCommandList, int cascadeIndex)
{
	pd3dCommandList->OMSetRenderTargets(0, nullptr, FALSE, &DsvHandles[cascadeIndex]);
	pd3dCommandList->ClearDepthStencilView(
		DsvHandles[cascadeIndex],
		D3D12_CLEAR_FLAG_DEPTH,
		1.0f, 0, 0, nullptr);
}

void ShadowMap::TransitionToSRV(ID3D12GraphicsCommandList* pd3dCommandList)
{
	if (ResourceState == D3D12_RESOURCE_STATE_DEPTH_WRITE)
	{
		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = ShadowMapResource;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		barrier.Transition.Subresource =
			D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

		pd3dCommandList->ResourceBarrier(1, &barrier);
		ResourceState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	}
}

void ShadowMap::TransitionToDSV(ID3D12GraphicsCommandList* pd3dCommandList)
{
	if (ResourceState == D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
	{
		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = ShadowMapResource;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
		barrier.Transition.Subresource =
			D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

		pd3dCommandList->ResourceBarrier(1, &barrier);
		ResourceState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	}
}

void ShadowMap::SetTextureOnParameter(ID3D12GraphicsCommandList* pd3dCommandlist)
{
	pd3dCommandlist->SetGraphicsRootDescriptorTable(15, SrvGpuHandle);
}

 