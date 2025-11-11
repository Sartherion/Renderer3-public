#pragma once
#include "Common.h"
#include "DepthBuffer.h"

struct DescriptorHeap;

struct ShadowMap
{
public:
	DepthBuffer depthBuffer;

	ComPtr<ID3D12PipelineState> pso;

	void Initialize(ID3D12Device10* device, uint32_t width, uint32_t height, uint32_t count, DXGI_FORMAT format, DescriptorHeap& descriptorHeap);

	void RenderBegin(ID3D12GraphicsCommandList10* commandList, uint32_t shadowMapIndex);

	void RenderEnd(ID3D12GraphicsCommandList10* commandList);

	void FrameEnd(ID3D12GraphicsCommandList10* commandList);
};

