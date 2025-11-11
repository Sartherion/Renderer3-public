#pragma once
#include "BufferMemory.h"
#include "Camera.h"
#include "DescriptorHeap.h"
#include "Light.h"

struct RenderSettings
{
	uint32_t renderTargetWidth = 3840 / 2;
	uint32_t renderTargetHeight = 2160 / 2; 
	uint32_t directionalLightsMaxCount = 4; 
	uint32_t shadowedPointLightsMaxCount = 12;
	uint32_t cubeMapsMaxCount = 1;
	uint32_t cascadedShadowMapSize = 1024 * 2;
	uint32_t omnidirectionalShadowMapSize = 1024;
	DXGI_FORMAT omndirectionalShadowMapsFormt = DXGI_FORMAT_D32_FLOAT;
	uint32_t cubeMapSize = 1024;
	DXGI_FORMAT cubeMapsFormat = DXGI_FORMAT_D32_FLOAT;
	uint32_t cubeMapFacesPerFrameUpdateCount = 1;
	DirectX::XMFLOAT3 ddgiProbeSpacing = { 3.0f, 3.0f, 3.0f };
	DirectX::XMFLOAT3 ddgiRelativeOffset = {};
	uint32_t accelerationStructureScratchBufferSizeBytes = 64 * 1024 * 1024;
};

struct PbrMesh;
namespace UI
{
	struct AppMenuBase;
}

struct RenderData 
{
	Camera::Transform cameraTransform;
	std::span<const PbrMesh*> opaqueMeshes;
	std::span<const PbrMesh*> shadowCasters; 
	std::span<const Light> directionalLights;
	std::span<const Light> pointLights;
	std::span<const Light> shadowedPointLights;
	uint32_t activeCubeMapsCount;
	BufferHeap::Offset cubeMapsTransformsOffset;
	DescriptorHeap::Id skyBoxSrvId;
	UI::AppMenuBase* appUIContext; //@todo: only needed to integrate app settings into renderer menu.
};

namespace UI
{
	struct AppMenuBase
	{
		virtual void MenuEntry() = 0;
	};
}

