#pragma once
#include "DescriptorHeap.h"
#include "Texture.h"

//RenderTargets always get srv and uav ids even if they don't use them 
struct RenderTarget : RWTexture
{
	RtvHeap::Allocation rtv;

	//if both depth buffer and render targets need to be bound to pipeline user either bind method but not both
	void Bind(ID3D12GraphicsCommandList10* commandList, D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = {}, uint32_t renderTargetArrayIndex = uint32_t(-1)) const;

	void Clear(ID3D12GraphicsCommandList10* commandList, uint32_t renderTargetArrayIndex = uint32_t(-1), const float(&clearColor)[4] = {}) const;

	RtvHandle GetRtv(uint32_t renderTargetArrayIndex = uint32_t(-1)) const;
};

//If an array of render targets is to be created, the function creates a rtv heap handle for each array element, so make sure that rtv heap is large enough
RenderTarget CreateRenderTarget(ID3D12Device10* device,
	const TextureProperties& properties,
	DescriptorHeap& srvHeap,
	LPCWSTR name = L"",
	TextureUsage usage = TextureUsage::Default,
	TextureMemoryType memoryType = TextureMemoryType::Default,
	D3D12_BARRIER_LAYOUT initialLayout = D3D12_BARRIER_LAYOUT_RENDER_TARGET,
	const D3D12_CLEAR_VALUE* clearValue = nullptr,
	TextureArrayType arrayType = TextureArrayType::Unspecified);

void DestroySafe(RenderTarget& renderTarget);

struct DepthBuffer;
void Bind(ID3D12GraphicsCommandList10* commandList,
	std::span<const RenderTarget> renderTargets,
	const DepthBuffer& depthBuffer,
	std::span<const uint32_t> renderTargetArrayIndices = {},
	uint32_t depthBufferArrayIndex = 0);

void Bind(ID3D12GraphicsCommandList10* commandList, std::span<const RenderTarget> renderTargets, std::span<const uint32_t> renderTargetArrayIndices = {});

void SetViewPort(ID3D12GraphicsCommandList10* commandList, const TextureProperties& properties);

D3D12_RENDER_TARGET_VIEW_DESC GetDefaultRtvDesc(const TextureProperties& textureProperties);

PingPong<RenderTarget> CreatePingPongRenderTarget(ID3D12Device10* device,
	const TextureProperties& properties,
	DescriptorHeap& srvHeap,
	LPCWSTR name = L"",
	TextureUsage usage = TextureUsage::Default);
