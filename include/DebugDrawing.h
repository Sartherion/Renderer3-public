#pragma once
#include "BufferMemory.h"

struct DescriptorHeap;
struct RenderTarget;
namespace DebugView
{
	extern RenderTarget renderTarget;

	void Init(ID3D12Device10* device,
		DescriptorHeap& descriptorHeap,
		BufferHeap& bufferHeap,
		DXGI_FORMAT renderTargetFormat,
		DXGI_FORMAT depthBufferFormat,
		uint32_t width,
		uint32_t height);

	void RenderBegin(ID3D12GraphicsCommandList10* commandList, BufferHeap::Offset cameraDataOffset, BufferHeap::Offset lightsDataBufferOffset);
	void RenderEnd(ID3D12GraphicsCommandList10* commandList);
}

struct ShadowMaps;
struct TextureResource;
namespace UI
{
	struct DebugVisualizationSettings
	{
		enum RenderMode : uint32_t
		{
			Default,
			Albedo,
			Normals,
			NormalsOctahedral,
			IndirectLighting,
			Ssao,
			Roughness,
			Metalness,
			Velocity,
			DepthLinear,
			ClusterLightCount,
			CascadeIndex,
			Count
		} renderMode = Default;

		inline static const char* NameLookup[] = { 
			"Default",
			"Albedo",
			"Normals",
			"NormalsOctahedral",
			"IndirectLighting", 
			"SSAO",
			"Roughness",
			"Metalness",
			"Velocity",
			"DepthLinear",
			"ClusterLightCount",
			"CascadeIndex"
		};

		int debugCascadeIndex = 0;
		int debugClusteredPerCellMaxCount = 5;
		bool isActiveDebugCamera = false;
		bool limitDebugCameraToMainCameraViewPort = true;
		bool isActiveDDGIVisualization = false;
		bool isRadianceDDGIVisualization = false;
		bool isActiveCSMVisualization = false;

		void MenuEntry();
	};

	bool DebugCameraWindow(TemporaryDescriptorHeap& descriptorHeap, const TextureResource& texture, bool& open);
}
