#pragma once
#include "BufferMemory.h"
#include "Camera.h"
#include "DepthBuffer.h"
#include "Frame.h"

constexpr int directionalLightsMaxCount = 4;
constexpr int cascadeCount = 5;
constexpr float cascadeSplits[cascadeCount] = { 0.02f, 0.08f, 0.15f, 0.3f, 1.0f };

struct LightsData 
{
	uint32_t pointLightsCount;
	BufferHeap::Offset pointLightsBufferOffset;
	uint32_t shadowedPointLightsCount;
	BufferHeap::Offset shadowedPointLightsBufferOffset;
	DescriptorHeap::Id omnidirectionalShadowMapsSrvId;
	uint32_t directionalLightsCount;
	BufferHeap::Offset directionalLightsBufferOffset;
	DescriptorHeap::Id cascadedShadowMapsSrvId;
	BufferHeap::Offset cascadeDataOffset;
	uint32_t cascadeCount;
};

struct LightingData
{
	LightsData lightsData;
	BufferHeap::Offset clusterDataOffset;
	BufferHeap::Offset ddgiDataOffset;
	DescriptorHeap::Id cubeMapSpecularId;
};

struct Light 
{
	DirectX::XMFLOAT3 color;
	DirectX::XMFLOAT3 position;
	DirectX::XMFLOAT3 direction;
	float fadeBegin;
	float fadeEnd;
};

struct ShadowedLight : Light
{
	BufferHeap::Offset shadowDataOffsest;
	uint32_t shadowMapArrayIndex;
};

struct DescriptorHeap;
struct PbrMesh;

namespace UI
{
	struct ShadowSettings;
}

struct ShadowMaps 
{
	void Init(ID3D12Device10* device,
		uint32_t arraySize,
		uint32_t shadowMapWidth,
		uint32_t shadowMapHeight,
		DXGI_FORMAT shadowMapFormat,
		DescriptorHeap& descriptorHeap,
		BufferHeap& bufferHeap,
		LPCWSTR name = L"ShadowMap");

	void Free();

	void RenderShadowMaps(ID3D12Device10* device,
		ID3D12GraphicsCommandList10* commandList,
		std::span<const PbrMesh*> meshList,
		uint32_t elementsCount,
		const UI::ShadowSettings& settings);

	[[nodiscard]]
	D3D12_TEXTURE_BARRIER Done();

	void RecompilePsoWithDepthBias(ID3D12Device10* device,
		int depthBias,
		float slopeScaledDepthBias,
		RasterizerState rasterizertState = RasterizerState::FrontFaceCull);

	DepthBuffer depthBuffer;
	DescriptorHeap::Id depthBufferCubeArraySrvId = DescriptorHeap::InvalidId;
	ComPtr<ID3D12PipelineState> pso;
	LPCWSTR name = L"";

	//GPU resident buffer
	FrameBuffered<PersistentBuffer<ShadowedLight>> lightsBuffer;
	FrameBuffered<PersistentBuffer<DirectX::XMFLOAT4X4>> transformsBuffer;
};

void UpdateLightDataCascade(std::span<const Light> lights,
	const DirectX::BoundingBox& boundingBoxGeometry,
	std::span<const DirectX::BoundingSphere> boundingSpheresFrusta,
	ShadowMaps& shadowMaps);

void UpdateShadowedPointLightsData(std::span<const Light> lights, ShadowMaps& shadowMaps);


void ComputeWorldSpaceSubFrustaBoundingBoxes(const float(&splitRatios)[cascadeCount],
	const Camera& camera,
	DirectX::CXMMATRIX inverseView,
	DirectX::BoundingBox(&boundingBoxes)[cascadeCount],
	DirectX::BoundingSphere(&boundingSpheres)[cascadeCount]);

namespace UI
{
	struct LightSettings;
}
LightsData ComputeLightsData(ScratchHeap& bufferHeap,
	const Camera& camera,
	std::span<const Light> directionalLights,
	std::span<const Light> shadowedPointLights,
	std::span<const Light> pointLights,
	const DirectX::BoundingBox& boundingBoxGeometry,
	ShadowMaps& cascadedShadowMaps,
	ShadowMaps& pointLightShadowMaps);

DirectX::BoundingBox ComputeCompoundMeshBoundingBox(std::span<const PbrMesh*> shadowCasters);

namespace UI
{
	struct LightingSettings
	{
		float iblSpecularIntensity = 1.0f;
		float iblDiffuseIntensity = 0.25f;
		float skyBrightness = 1.0f;
		float shadowDarkness = 1.0f;
		bool useSpecular = true;

		void MenuEntry();
	};

	struct ShadowSettings
	{
		int depthBias = 1200;
		float slopeScaledDepthBias = 1.8f;
		enum ComboCullSettings : int
		{
			None,
			FrontFace,
			BackFace,
			Count
		} cullSettings = FrontFace;
		bool shadowSettingsApplied = true;

		void MenuEntry(LPCSTR name);
	};

}
