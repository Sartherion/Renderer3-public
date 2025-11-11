#pragma once
#include "BufferMemory.h"
#include "DescriptorHeap.h"

struct PbrMesh;

struct RenderFeatures 
{
	union
	{
		struct
		{
			uint32_t useDirectDirectionalLights : 1 = true;
			uint32_t useDirectPointLights : 1 = true;
			uint32_t sampleSpecularCubeMap : 1 = false;
			uint32_t sampleDiffuseDDGI : 1 = true;
			uint32_t sampleSpecularDDGI : 1 = true;
			uint32_t sampleIndirectDiffuseMap : 1 = false;
			uint32_t sampleIndirectSpecularMap : 1 = false;
			uint32_t sampleAoMap : 1 = false;
			uint32_t sampleShadowMap : 1 = true;
			uint32_t forceLastShadowCascade : 1 = false;
			uint32_t isDebugCamera : 1 = false;
			//@note: it does not make sense to toggle alpha testing with a runtime switch, since early z will be disabled if the shader may use clip
		} asBitfield;
		uint32_t asUint;
	};
};


namespace Draw
{
	struct ParametersOpaque
	{
		DescriptorHeap::Id ssaoBufferSrvId = DescriptorHeap::InvalidId;
		DescriptorHeap::Id sssrBufferSrvId = DescriptorHeap::InvalidId;
		DescriptorHeap::Id indirectDiffuseBufferSrvId = DescriptorHeap::InvalidId;
	};

	// assumes that cameraData is bound to slot9 and renderFeatures to slot8
	void Opaque(ID3D12GraphicsCommandList10* commandList, ParametersOpaque parameters, std::span<const PbrMesh*> meshList, bool useDefaultPso = true);
	void SkyBox(ID3D12GraphicsCommandList10* commandList, DescriptorHeap::Id skyBoxTextureSrvId);
}

void InitDrawHelpers(ID3D12Device10* device, BufferHeap& bufferHeap, DXGI_FORMAT renderTargetFormat, DXGI_FORMAT depthBufferFormat);

struct Geometry;

namespace BasicShapes 
{
	extern Geometry sphere;
	extern Geometry cube;
}

void BindFixedRenderGraphicsRootConstants(ID3D12GraphicsCommandList10* commandList,
	BufferHeap::Offset lightingDataBufferOffset,
	BufferHeap::Offset cameraConstantsBufferOffset,
	BufferHeap::Offset dimensionsBufferOffset,
	RenderFeatures renderFeatures);
