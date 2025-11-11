#pragma once
#include "BufferMemory.h"
#include "D3DUtility.h"

struct BufferHeap;
struct DescriptorHeap;
struct RenderTarget;
struct DepthBuffer;

namespace GBuffer
{
	struct SrvIds
	{
		DescriptorHeap::Id albedo;
		DescriptorHeap::Id previousAlbedo;
		DescriptorHeap::Id normals;
		DescriptorHeap::Id previousNormals;
		DescriptorHeap::Id velocities;
		DescriptorHeap::Id depth;
		DescriptorHeap::Id previousDepth;
		DescriptorHeap::Id depthPyramide;
	};

	extern PingPong<RenderTarget> albedoRT;
	extern PingPong<RenderTarget> normalsRT;
	extern RenderTarget velocitiesRT;
	extern PingPong<DepthBuffer> depthBuffer;
	extern RWTexture depthPyramide;

	void Init(ID3D12Device10* device,
		uint32_t renderTargetWidth,
		uint32_t renderTargetHeight,
		DXGI_FORMAT depthBufferFormat,
		DescriptorHeap& descriptorHeap,
		BufferHeap& bufferHeap);

	void RenderBegin(ID3D12GraphicsCommandList10* commandList, BufferHeap::Offset cameraDataOffset);
	void RenderEnd(ID3D12GraphicsCommandList10* commandList, TemporaryDescriptorHeap& descriptorHeap);

	void Lighting(ID3D12GraphicsCommandList10* commandList,
		const RWTexture& output,
		DescriptorHeap::Id ssaoBufferSrvId,
		DescriptorHeap::Id indirectDiffuseBufferSrvId,
		DescriptorHeap::Id sssrBufferSrvId,
		BufferHeap::Offset lightingDataBufferOffset);

	void FrameEnd(ID3D12GraphicsCommandList10* commandList);

	SrvIds GetSrvIds();
}

namespace UI
{
	struct MaterialSettings
	{
		bool useAlbedoMaps = true;
		bool useNormalMaps = true;
		bool useRoughnessMaps = true;
		bool useMetallicMaps = true;
		float defaultRoughness = 1.0f;;
		float defaultMetalness = 0.0f;

		void MenuEntry();
	};
}

