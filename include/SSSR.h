#pragma once
#include "BufferMemory.h"
#include "DescriptorHeap.h"

struct DescriptorHeap;
namespace SSSR
{
	inline DescriptorHeap::Id bufferSrvId;

	void Init(ID3D12Device10* device, uint32_t renderTargetWidth, uint32_t renderTargetHeight, DescriptorHeap& descriptorHeap, BufferHeap& bufferHeap);
	void Render(ID3D12GraphicsCommandList10* commandList, DescriptorHeap::Id previousLitBufferSrvId, BufferHeap::Offset lightsDataBufferOffset);
	void FrameEnd(ID3D12GraphicsCommandList10* commandList);
}

namespace UI
{
	struct SSSRSettings
	{
		int maxHierarchicalSteps = 200;
		int maxConetracingMip = 10;
		float intensity = 1.0f;
		float bias = 0.5f;
		float depthBufferThickness = 0.5f;
		float depthNormalSimilarityThreshold = 0.99f;
		float borderFadeStart = 0.5f;
		float borderFadeEnd = 1.0f;
		float parallaxThreshold = 0.05f;
		float historyWeight = 0.95f;
		float iblFallbackRougnessThresholdBegin = 0.3f;
		float iblFallbackRougnessThresholdEnd = 0.4f;

		void MenuEntry();
	};
}
