#pragma once
#include "DescriptorHeap.h"
#include "Texture.h"

struct DepthBuffer : RWTexture
{
	DsvHeap::Allocation dsv;

	//if both depth buffer and render targets need to be bound to pipeline user either bind method but not both
	void Bind(ID3D12GraphicsCommandList10* commandList, D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = {}, uint32_t depthBufferArrayIndex = uint32_t(-1)) const;

	void Clear(ID3D12GraphicsCommandList10* commandList, uint32_t depthBufferArrayIndex = uint32_t(-1), float clearValue = 1.0f) const;

	DsvHandle GetDsv(uint32_t depthBufferArrayIndex = uint32_t(-1));
};

//If an array of depth buffers is to be created, the function creates a dsv heap handle for each array element, so make sure that dsv heap is large enough
DepthBuffer CreateDepthBuffer(ID3D12Device10* device,
	const TextureProperties& properties,
	DescriptorHeap& srvHeap,
	LPCWSTR name = L"",
	TextureUsage usage = TextureUsage::Default,
	TextureMemoryType memoryType = TextureMemoryType::Default,
	D3D12_BARRIER_LAYOUT initialLayout = D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE,
	const D3D12_CLEAR_VALUE* clearValue = nullptr,
	TextureArrayType arrayType = TextureArrayType::Unspecified);

void DestroySafe(DepthBuffer& depthBuffer);

D3D12_DEPTH_STENCIL_VIEW_DESC GetDefaultDsvDesc(const TextureProperties& textureProperties);

PingPong<DepthBuffer> CreatePingPongDepthBuffer(ID3D12Device10* device,
	const TextureProperties& properties,
	DescriptorHeap& srvHeap,
	LPCWSTR name = L"",
	TextureUsage usage = TextureUsage::Default);

