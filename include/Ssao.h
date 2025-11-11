#pragma once
#include "BufferMemory.h"
#include "DescriptorHeap.h"

struct BufferHeap;
namespace SSAO
{
	inline DescriptorHeap::Id bufferSrvId;

	void Init(ID3D12Device10* device, DescriptorHeap& descriptorHeap, uint32_t width, uint32_t height);
	void Render(ID3D12GraphicsCommandList10* commandList, BufferHeap::Offset ddgiDataOffset, DescriptorHeap::Id litBufferSrvId);
	[[nodiscard]]
	D3D12_TEXTURE_BARRIER Done();
}

namespace UI
{
	struct SSAOSettings
	{
		float thickness = 0.75f;
		float radius = 0.25f;
		int stepsCount = 4;
		int azimuthalDirectionsCount = 1;
		float blurDepthSigma = 0.5f;
		float blurNormalSigma = 0.5f;
		bool isActiveAO = true;
		bool useOffsetJitter = true;

		void MenuEntry();
	};
}