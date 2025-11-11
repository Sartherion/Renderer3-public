#pragma once
#include "RenderTarget.h"
#include "BufferMemory.h"


struct LightsData;

enum LightType : uint32_t;

class ClusteredShadingContext
{
public:
	void Init(ID3D12Device10* device,
		DescriptorHeap& descriptorHeap,
		BufferHeap& bufferHeap,
		uint32_t renderTargetWidth,
		uint32_t renderTargetHeight,
		LPCWSTR name = L"",
		uint32_t maximumLightsPerShellPass = 2048);

	void PrepareClusterData(ID3D12GraphicsCommandList10* commandList,
		TemporaryDescriptorHeap& descriptorHeap,
		const LightsData& lightsData,
		BufferHeap::Offset cameraDataOffset,
		const RWTexture* depthPyramide = nullptr);

	BufferHeap::Offset GetClusterDataBufferOffset() const
	{
		return clusterDataBuffer.Offset();
	}

	void Free();

private:
	struct ClusterData
	{
		DescriptorHeap::Id clusteredLightListHeadNodesId = DescriptorHeap::InvalidId;
		DescriptorHeap::Id clusteredLinkedLightListId = DescriptorHeap::InvalidId;
		uint32_t clusterCountX;
		uint32_t clusterCountY;
		uint32_t clusterCountZ;
	};

	uint32_t maxLightsPerShellPass;
	uint32_t maxLightsCount;
	uint32_t maxLightNodeCount;
	uint32_t clusterCountX;
	uint32_t clusterCountY;
	uint32_t clusterCount;

	RenderTarget lightShellRT;

	RWRawBufferWithCounter linkedLightListBuffer;
	RWRawBuffer lightsHeadNodesBuffer;

	PersistentBuffer<ClusterData> clusterDataBuffer;

	wchar_t name[32];

	void RenderShellPass(ID3D12GraphicsCommandList10* commandList,
		LightType lightType,
		BufferHeap::Offset lightsDataOffset,
		uint32_t lightsCount,
		BufferHeap::Offset cameraDataOffset);

	void FillPass(ID3D12GraphicsCommandList10* commandList,
		TemporaryDescriptorHeap& descriptorHeap,
		LightType lightType,
		uint32_t pointLightsCount,
		BufferHeap::Offset cameraDataOffset,
		const RWTexture* depthPyramide);
};
