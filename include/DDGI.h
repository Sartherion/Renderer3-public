#pragma once
#include "BufferMemory.h"

struct BufferHeap;
struct Geometry;
struct TlasData;

namespace DDGI
{
	inline BufferHeap::Offset bufferOffset;

	struct InitData 
	{
		uint32_t depthAtlasTileSize = 18;
		uint32_t depthAtlasDimensionX = 4096;
		uint32_t depthAtlasDimensionY = 4096;
		uint32_t radianceAtlasTileSize = 18;
		uint32_t radianceAtlasDimensionX = 4096;
		uint32_t radianceAtlasDimensionY = 4096;
		uint32_t irradianceAtlasTileSize = 10;
		uint32_t irradianceAtlasDimensionX = 2048;
		uint32_t irradianceAtlasDimensionY = 2048;
		DirectX::XMUINT3 gridDimensions = { 8, 8, 8 };
		DirectX::XMFLOAT3 probeSpacing = { 3.0f, 3.0f, 3.0f };
		DirectX::XMFLOAT3 relativeOffset = { 0.0f, 0.0f, 0.0f }; // offset of center in units of overall grid size
	};

	void Init(ID3D12Device10* device, DescriptorHeap& descriptorHeap, BufferHeap& bufferHeap, InitData&& initData);

	void Render(ID3D12GraphicsCommandList10* commandList, BufferHeap::Offset lightsDataOffset, const TlasData& tlasData, DescriptorHeap::Id skySrvId);

	void DrawDebugVisualization(ID3D12GraphicsCommandList10* commandList);

	void FrameEnd(ID3D12GraphicsCommandList10* commandList);
}

namespace UI
{
	struct DDGISettings
	{
		float higherBounceIndirectDiffuseIntensity = 0.9f;
		float higherBounceIndirectSpecularIntensity = 0.98f;
		float irradianceGammaExponent = 1.0f;
		float historyBlendWeight = 0.985f;

		void MenuEntry();
	};

}
