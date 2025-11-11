#pragma once
#include "BufferMemory.h"
#include "DepthBuffer.h"
#include "Light.h"
#include "PassIterator.h"
#include "RenderTarget.h"
#include "Texture.h"

struct DescriptorHeap;

struct CubeMaps
{
	RenderTarget renderTargets;
	DepthBuffer depthBuffer;
	RWTexture downsamplingTemporaryTexture;

	PersistentBuffer<Texture2DDimensions> dimensionsBuffer;

	TemporaryBuffer<LightingData> lightingDataBuffer;

	int perFrameFaceUpdatesCount;
	int lastUpdatedFace = 0;

	void Init(ID3D12Device10* device,
		uint32_t size,
		uint32_t cubeMapCount,
		uint32_t mipCount,
		DXGI_FORMAT renderFormat,
		DXGI_FORMAT depthFormat,
		DescriptorHeap& descriptorHeap,
		BufferHeap& bufferHeap);

	void Free();

	void RenderBegin(ID3D12GraphicsCommandList10* commandList, ScratchHeap& scratchHeap, const LightingData& lightingData);

	void PassBegin(ID3D12GraphicsCommandList10* commandList,
		TemporaryDescriptorHeap & temporaryDescriptorHeap,
		const LightingData& lightingData,
		BufferHeap::Offset cameraDataBaseOffset,
		uint32_t index) const;

	void PassEnd(ID3D12GraphicsCommandList10* commandList, uint32_t index) const;

	void RenderEnd(ID3D12GraphicsCommandList10* commandList, TemporaryDescriptorHeap& temporaryDescriptorHeap, uint32_t activeCubeMapsFaceCount);

	[[nodiscard]]
	D3D12_TEXTURE_BARRIER Done() const;
};

//Definitions for Iterating over CubeMap array elements using range-based for
struct CubeMapsIteratorExtraFields
{
	TemporaryDescriptorHeap& temporaryDescriptorHeap;
	ScratchHeap& scratchHeap;
	LightingData& lightingData;
	BufferHeap::Offset cameraDataBaseOffset;
	uint32_t activeCubeMapsCount = uint32_t(-1);
};

using AllCubeMaps = Iterate<CubeMaps, CubeMapsIteratorExtraFields>;

template<>
inline void RenderBegin(AllCubeMaps& pass)
{
	pass.iterationBegin = pass.collection.lastUpdatedFace;

	const uint32_t activeCubeMapsFaceCount = Min(pass.extraFields.activeCubeMapsCount * 6, pass.collection.renderTargets.properties.arraySize);
	pass.iterationEnd = activeCubeMapsFaceCount > 0 ? (pass.collection.lastUpdatedFace + pass.collection.perFrameFaceUpdatesCount) : pass.iterationBegin;
	pass.collection.RenderBegin(pass.commandList, pass.extraFields.scratchHeap, pass.extraFields.lightingData);
}

template<>
inline void PassBegin(AllCubeMaps& pass, uint32_t index)
{
	pass.collection.PassBegin(pass.commandList, pass.extraFields.temporaryDescriptorHeap, pass.extraFields.lightingData, pass.extraFields.cameraDataBaseOffset, index);
}

template<>
inline void RenderEnd(AllCubeMaps& pass)
{
	const uint32_t activeCubeMapsFaceCount = Min(pass.extraFields.activeCubeMapsCount * 6, pass.collection.renderTargets.properties.arraySize);
	pass.collection.RenderEnd(pass.commandList, pass.extraFields.temporaryDescriptorHeap, activeCubeMapsFaceCount);
	if (activeCubeMapsFaceCount > 0)
	{
		pass.collection.lastUpdatedFace = (pass.collection.lastUpdatedFace + pass.collection.perFrameFaceUpdatesCount) % activeCubeMapsFaceCount;
	}
}


DirectX::XMMATRIX XM_CALLCONV BuildCubeMapViewMatrix(DirectX::FXMVECTOR position, int axis);
DirectX::XMMATRIX XM_CALLCONV CalculateCubeMapViewProjection(int planeIndex, const DirectX::FXMVECTOR position, float nearPlane, float farPlane);

void UpdateCubeMapCameraData(BufferHeap& bufferHeap,
	BufferHeap::Offset cameraDataOffset,
	const DirectX::XMFLOAT3& cubeMapPosition,
	float nearPlane = 0.1f,
	float farPlane = 1000.0f);
