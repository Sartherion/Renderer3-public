#include "stdafx.h"
#include "Random.h"

#include "BufferMemory.h"
#include "Texture.h"

namespace BlueNoiseGeneration
{
	static ComPtr<ID3D12PipelineState> blueNoiseGenerationPso;
	static RWTexture blueNoise;
	static PersistentBuffer<int> sobolBuffer;
	static PersistentBuffer<int> scramblingTileBuffer;
	static PersistentBuffer<int> rankingTileBuffer;

	void Init(ID3D12Device10* device, DescriptorHeap& descriptorHeap, BufferHeap& bufferHeap)
	{
		blueNoiseGenerationPso = CreateComputePso(device, { .cs = LoadShaderBinary(L"content\\shaderbinaries\\BlueNoiseGenerationCS.cso").Get() });
		blueNoise = CreateRWTexture(device,
			{ .format = DXGI_FORMAT_R16G16_UNORM, .width = 128, .height = 128 },
			descriptorHeap,
			L"Blue Noise");

		sobolBuffer = CreatePersistentBuffer<int>(bufferHeap, _countof(sobol_256spp_256d));
		scramblingTileBuffer = CreatePersistentBuffer<int>(bufferHeap, _countof(scramblingTile));
		rankingTileBuffer = CreatePersistentBuffer<int>(bufferHeap, _countof(rankingTile));

		sobolBuffer.Write(sobol_256spp_256d);
		scramblingTileBuffer.Write(scramblingTile);
		rankingTileBuffer.Write(rankingTile);

		texture = &blueNoise;
		
	}

	void Generate(ID3D12GraphicsCommandList10* commandList)
	{
		PIXScopedEvent(commandList, PIX_COLOR_DEFAULT, "Blue Noise Generation");

		const uint32_t textureSize = blueNoise.properties.width;
		assert(textureSize == blueNoise.properties.height);

		DispatchComputePass(commandList,
			blueNoiseGenerationPso.Get(),
			{
				blueNoise.uavId,
				sobolBuffer.Offset(),
				scramblingTileBuffer.Offset(),
				rankingTileBuffer.Offset()
			},
			{ .dispatchX = textureSize, .dispatchY = textureSize});

		ResourceTransitions(commandList,
			{
				blueNoise.Barrier(ResourceState::WriteCS, ResourceState::ReadCS),
			}
		);
	}

	D3D12_TEXTURE_BARRIER Done()
	{
		return blueNoise.Done(ResourceState::ReadCS, D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_UNORDERED_ACCESS);
	}
}
