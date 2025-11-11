#pragma once
#include "BufferMemory.h"

struct DescriptorHeap;
struct RWTexture;
struct Texture;
struct TlasData;

namespace UI
{
	struct ReferencePathTracerSettings;
}

namespace PathTracer
{
	void Init(ID3D12Device10* device, DescriptorHeap& descriptorHeap, uint32_t renderWidth, uint32_t renderHeight);

	Texture* Render(ID3D12GraphicsCommandList10* commandList,
		RWTexture& temporaryBuffer,
		const TlasData& tlasData,
		DescriptorHeap::Id skyboxSrvId,
		BufferHeap::Offset lightsDataOffset,
		const UI::ReferencePathTracerSettings& settings);

	[[nodiscard]]
	D3D12_TEXTURE_BARRIER Done();
}

namespace UI
{
	struct ReferencePathTracerSettings
	{
		bool isActive = false;
		bool denoise = false;
		bool boostRoughness = true;
		bool castShadowsForUnshadowedLights = false;
		int bounceMaximumCount = 20;
		float denoiseNormalSigma = 0.5f;
		float denoiseDepthSigma = 0.5f;

		void MenuEntry();
	};
}
