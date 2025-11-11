#include "stdafx.h"
#include "App.h"

#include "AppUI.h"
#include "BufferMemory.h"
#include "CubeMap.h"
#include "Geometry.h"
#include "Texture.h"

namespace App
{
	static std::vector<const PbrMesh*> opaqueMeshes;
	static std::vector<const PbrMesh*> shadowCasters;
	static PersistentBuffer<Camera::Constants> cubeMapsCameraData;

	static PbrMesh meshSponza;
	static PbrMesh meshSphere;
	static Texture textureSkybox;

	static const int shadowedPointLightsCount = 1;
	static Light shadowedPointLights[shadowedPointLightsCount] = {};

	static const int unshadowedPointLightsCount = 2 * 1024;
	static Light unshadowedPointLights[unshadowedPointLightsCount] = {};

	Light directionalLights[renderSettings.directionalLightsMaxCount];

	static UIContext uiContext{};

	void Init(ID3D12Device10* device,
		ID3D12GraphicsCommandList10* commandList,
		PersistentAllocator& allocator,
		DescriptorHeap& descriptorHeap,
		BufferHeap& bufferHeap,
		RWBufferResource& scratchBuffer)
	{
		menu = &uiContext;
		textureSkybox = LoadTexture(L"content\\textures\\skybox.dds", device, descriptorHeap);

		meshSponza = LoadMesh(device, allocator, descriptorHeap, bufferHeap, L"content\\geometry\\sponza2.obj");

		meshSphere = LoadMesh(device, allocator, descriptorHeap, bufferHeap, L"content\\geometry\\sphere.obj");
		SetMaterial(meshSphere, { .metallic = 1.0f, .roughness = 0.0f, .specularCubeMapsArrayIndex = 0 });

		opaqueMeshes = { &meshSponza, &meshSphere };
		shadowCasters = opaqueMeshes;

		for (int i = 0; i < shadowedPointLightsCount; i++)
		{
			shadowedPointLights[i] =
			{
				.color = {0.25f * i, 0.5f, 0.75f * i },
				.position = { -6.0f + i * 2.0f, 5.0f, 0.0f },
				.fadeBegin = 3.0f,
				.fadeEnd = 6.f
			};
		}

		shadowedPointLights[0] =
		{
			.color = {1.25f , 1.5f, 0.75f },
			.position = {  0.0f, 3.0f, 0.0f },
			.fadeBegin = 6.0f,
			.fadeEnd = 8.f
		};


		srand(0);
		for (int i = 0; i < unshadowedPointLightsCount; i++)
		{
			unshadowedPointLights[i] =
			{
				.color = { RandFloat(), RandFloat(), RandFloat() },
				.position = { 12.0f * sinf(RandFloat() * 2 * DirectX::XM_PI) ,
					6.0f - 6.0f * sinf(RandFloat() * 2 * DirectX::XM_PI),
					7.5f * sinf(RandFloat() * 2 * DirectX::XM_PI)  },
				.fadeBegin = 2.0f,
				.fadeEnd = 3.0f
			};
		}

		//Initialize per object data
		DirectX::XMFLOAT4X4 transform, inverseTransform;
		const float sphereY = 1;
		const DirectX::XMMATRIX sphereTransform = DirectX::XMMatrixTranslation(0, sphereY, 0);
		DirectX::XMStoreFloat4x4(&transform, DirectX::XMMatrixTranspose(sphereTransform));
		DirectX::XMStoreFloat4x4(&inverseTransform, DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(nullptr, sphereTransform)));

		PbrMesh::InstanceData instanceData[1];
		instanceData[0] = { transform, inverseTransform };
		InitPersistentInstanceData(meshSphere, allocator, bufferHeap, instanceData);

		cubeMapsCameraData = CreatePersistentBuffer<Camera::Constants>(bufferHeap, 6 * renderSettings.cubeMapsMaxCount);

		UpdateCubeMapCameraData(bufferHeap, cubeMapsCameraData.offset, { 0, sphereY, 0 } );

		meshSponza.BuildBlas(device, commandList,scratchBuffer);
		meshSphere.BuildBlas(device, commandList, scratchBuffer);
	}

	RenderData Update(LinearAllocator& frameAllocator, const Frame::TimingData& timingData)
	{
		uint32_t activeDirectionalLightsCount = uiContext.lightSettings.ReadDirectionalLightData(directionalLights);
		assert(activeDirectionalLightsCount <= directionalLightsMaxCount);

		const float elapsedTime = static_cast<float>(timingData.elapsedTimeMs);

		Camera::Transform cameraTransform = ProcessInput(timingData.deltaTimeMs, !UI::mouseOverUI);

		const float sphereX = 0 * 4 * std::sin(elapsedTime *  2.5e-3f) + 4.5f;
		const DirectX::XMMATRIX sphereTransform = DirectX::XMMatrixTranslation(sphereX, 1.0f, 0);
		DirectX::XMFLOAT4X4 transform, inverseTransform;
		DirectX::XMStoreFloat4x4(&transform, DirectX::XMMatrixTranspose(sphereTransform));
		DirectX::XMStoreFloat4x4(&inverseTransform, DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(nullptr, sphereTransform)));
		SetTemporaryInstanceData(meshSponza, Frame::current->cpuMemory, Frame::current->gpuMemory, { { PbrMesh::InstanceDataDefault } });
		SetTemporaryInstanceData(meshSphere, Frame::current->cpuMemory, Frame::current->gpuMemory, { { PbrMesh::InstanceData{ transform, inverseTransform } } });

#if 0 //dynamic shadowed point lights
		srand(0);
		for (int i = 0; i < shadowedPointLightsCount; i++)
		{
			shadowedPointLights[i] =
			{
				.color = { RandFloat(), RandFloat(), RandFloat() },
				.position = { 10.0f * sinf(0.001f * RandFloat() * elapsedTime + RandFloat() + RandFloat() * 2 * DirectX::XM_PI),
					3.0f - 2.5f * cosf(0.001f * RandFloat() * elapsedTime + RandFloat() + RandFloat() * 2 * DirectX::XM_PI),
					5.5f * cosf(0.001f * RandFloat() * elapsedTime + RandFloat() * 2 * DirectX::XM_PI)  },
				.fadeBegin = 3.0f,
				.fadeEnd = 5.0f
			};
		}
#endif

		// dynamic unshadowed point lights
		srand(0);
		const uint32_t dynamicPointLightsCount = 0 * 2 * 1024;
		Light* dynamicPointLights = frameAllocator.Allocate<Light>(dynamicPointLightsCount);
		for (int i = 0; i < dynamicPointLightsCount; i++)
		{
			dynamicPointLights[i] =
			{
				.color = { RandFloat(), RandFloat(), RandFloat() },
				.position = { 12.0f * sinf(0.001f * RandFloat() * elapsedTime + RandFloat() + RandFloat() * 2 * DirectX::XM_PI),
					6.0f - 6.0f * cosf(0.001f * RandFloat() * elapsedTime + RandFloat() + RandFloat() * 2 * DirectX::XM_PI),
					7.5f * cosf(0.001f * RandFloat() * elapsedTime + RandFloat() * 2 * DirectX::XM_PI)  },
				.fadeBegin = 1.5f,
				.fadeEnd = 2.5f
			};
		}

		DirectX::XMFLOAT3* cubeMapPositions = frameAllocator.Allocate<DirectX::XMFLOAT3>(1);
		cubeMapPositions[0] = { sphereX, 0.0f, 0.0f };
		return RenderData
		{
			.cameraTransform = cameraTransform,
			.opaqueMeshes = opaqueMeshes,
			.shadowCasters = shadowCasters,
			.directionalLights = { directionalLights, activeDirectionalLightsCount },
			.pointLights = { dynamicPointLights, dynamicPointLightsCount },
			//.pointLights = { unshadowedPointLights, _countof(unshadowedPointLights)},
			.shadowedPointLights =  shadowedPointLights,
			.activeCubeMapsCount = 1,
			.cubeMapsTransformsOffset = cubeMapsCameraData.offset,
			.skyBoxSrvId = textureSkybox.srvId,
			.appUIContext = &uiContext
		};
	}
}
