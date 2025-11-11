#pragma once
#include "D3DBarrierHelpers.h"
#include "BufferMemory.h"

struct TlasData;
namespace UI
{
	struct IndirectDiffuseSettings;
}

namespace IndirectDiffuse
{
	inline DescriptorHeap::Id bufferSrvId;

	void Init(ID3D12Device10* device, uint32_t renderWidth, uint32_t renderHeight, DescriptorHeap& descriptorHeap);

	void Render(ID3D12GraphicsCommandList10* commandList,
		TemporaryDescriptorHeap& descriptorHeap,
		const TlasData& tlasData,
		DescriptorHeap::Id skyboxsrvId,
		BufferHeap::Offset lightsDataOffset,
		BufferHeap::Offset aoBufferOffset,
		const UI::IndirectDiffuseSettings& settings);
}

namespace UI
{
	struct IndirectDiffuseSettings
	{
		float rtaoHitDistanceThreshold = 0.3f;
		float blurRadius = 1.0f;
		float normalWeightExponent = 8.0f;
		float preBlurRadius = 0.5f;
		float disocclusionDepthThreshold = 0.03;
		float historyFixBilateralDepthScale = 0.5f;
		bool denoiseInSHSpace = false;
		bool resetHistoryThisFrame = false; 
		bool isActive = true;

		void MenuEntry();
	};
}
