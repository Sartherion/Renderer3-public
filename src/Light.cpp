#include "stdafx.h"
#include "Light.h"

#include "CubeMap.h" 
#include "Geometry.h"
#include "SharedDefines.h"

static DirectX::XMMATRIX XM_CALLCONV ComputeTightViewMatrix(DirectX::FXMVECTOR lightDirection,
	const DirectX::BoundingBox& boundingBoxGeometry,
	const DirectX::BoundingSphere& boundingSphere,
	uint32_t textureSize);

static DirectX::XMMATRIX XM_CALLCONV ComputeTightOrthogonalViewProjectionMatrix(DirectX::FXMVECTOR lightDirection,
	const DirectX::BoundingBox& boundingBoxGeometry,
	const DirectX::BoundingSphere& boundingSphere,
	uint32_t textureSize);

static ComPtr<ID3D12PipelineState> CreatePso(ID3D12Device10* device,
	DXGI_FORMAT format,
	int depthBias,
	float slopeScaledDepthBias,
	RasterizerState rasterizertState = RasterizerState::FrontFaceCull);

static void UpateShadowMaps(ShadowMaps& instance, const UI::ShadowSettings& settings, ID3D12Device10* device);

void ShadowMaps::Init(ID3D12Device10* device,
	uint32_t arraySize,
	uint32_t shadowMapWidth,
	uint32_t shadowMapHeight,
	DXGI_FORMAT shadowMapFormat,
	DescriptorHeap& descriptorHeap,
	BufferHeap& bufferHeap,
	LPCWSTR name)
{
	this->name = name;

	for (auto& element : lightsBuffer)
	{
		element = CreatePersistentBuffer<ShadowedLight>(bufferHeap, arraySize);
	}
	for (auto& element : transformsBuffer)
	{
		element = CreatePersistentBuffer<DirectX::XMFLOAT4X4>(bufferHeap, arraySize);
	}

	depthBuffer = CreateDepthBuffer(device,
		{ .format = shadowMapFormat, .width = shadowMapWidth, .height = shadowMapHeight, .arraySize = arraySize },
		descriptorHeap,
		name);

	depthBufferCubeArraySrvId = CreateSrvOnHeap(descriptorHeap,	depthBuffer, TextureArrayType::CubeMapArray); //needed for omni shadowmap sampling

	pso = CreatePso(device, shadowMapFormat, 5000, 2.0f);
}

void ShadowMaps::Free()
{
	for (auto& element : lightsBuffer)
	{
		element.Free();
	}
	for (auto& element : transformsBuffer)
	{
		element.Free();
	}
	DestroySafe(depthBuffer);
	Frame::SafeRelease(std::move(pso));
}

void ShadowMaps::RenderShadowMaps(ID3D12Device10* device,
	ID3D12GraphicsCommandList10* commandList,
	std::span<const PbrMesh*> meshList,
	uint32_t elementsCount,
	const UI::ShadowSettings& settings)
{
	UpateShadowMaps(*this, settings, device);
	using namespace DirectX;

	assert(elementsCount <= depthBuffer.properties.arraySize);

	PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, "%ls", name); //@note: l because PIX functions expext char* but we pass it a wchar_t*
	for (uint32_t i = 0; i < elementsCount; i++)
	{
		//Render shadow map
		PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, "Pass %d", i);
		commandList->SetPipelineState(pso.Get());
		depthBuffer.Clear(commandList, i);
		depthBuffer.Bind(commandList, {}, i);
		
		commandList->SetGraphicsRoot32BitConstant(0, transformsBuffer->Offset(i), 9);
		for (const auto* mesh : meshList)
		{
			mesh->DrawOneDrawcall(commandList);
		}
		PIXEndEvent(commandList);
	}
	ResourceTransitions(commandList, { depthBuffer.Barrier(ResourceState::DepthWrite, ResourceState::ReadPS) });
	PIXEndEvent(commandList);
}

D3D12_TEXTURE_BARRIER ShadowMaps::Done()
{
	return depthBuffer.Done(ResourceState::ReadPS, D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE);
}

void ShadowMaps::RecompilePsoWithDepthBias(ID3D12Device10* device, int depthBias, float slopeScaledDepthBias, RasterizerState rasterizertState)
{
	Frame::SafeRelease(std::move(pso));
	pso = CreatePso(device, depthBuffer.properties.format, depthBias, slopeScaledDepthBias, rasterizertState);
}

ComPtr<ID3D12PipelineState> CreatePso(ID3D12Device10* device,
	DXGI_FORMAT format,
	int depthBias,
	float slopeScaledDepthBias,
	RasterizerState rasterizertState)
{
	auto shadowCasterVs = LoadShaderBinary(L"content\\shaderbinaries\\ShadowCasterVS.cso");
	auto rasterizerState = GetRasterizerState(rasterizertState);
	rasterizerState.DepthBias = depthBias; 
	rasterizerState.SlopeScaledDepthBias = slopeScaledDepthBias;
	return CreateGraphicsPso(device,
		{
			.vs = shadowCasterVs.Get(),
			.rasterizerState = rasterizerState,
			.depthState = GetDepthState(DepthState::Write),
			.dsvFormat = format,
		});
}

DirectX::XMMATRIX XM_CALLCONV ComputeTightViewMatrix(DirectX::FXMVECTOR lightDirection,
	const DirectX::BoundingBox& boundingBoxGeometry, 
	const DirectX::BoundingSphere& boundingBoxFrustum,
	uint32_t textureSize)
{
	using namespace DirectX;
	XMVECTOR center = XMLoadFloat3(&boundingBoxFrustum.Center);

	XMVECTOR normalizedLightDirection = XMVector4Normalize(lightDirection);
	XMVECTOR lightUp = FindPerpendicularUnitVector(normalizedLightDirection);

	//@note: following https://alextardif.com/shadowmapping.html to snap view matrix to texel increments
	const float texelsPerWorldUnit = textureSize / (2.0f * boundingBoxFrustum.Radius); 
	XMMATRIX scalingMatrix = XMMatrixScaling(texelsPerWorldUnit, texelsPerWorldUnit, texelsPerWorldUnit);
	XMMATRIX lookAtMatrix = XMMatrixMultiply(scalingMatrix, XMMatrixLookAtLH(normalizedLightDirection, {}, lightUp));
	XMMATRIX inverseLookAtMatrix = XMMatrixInverse(nullptr, lookAtMatrix);

	center = XMVector3Transform(center, lookAtMatrix);
	center = XMVectorSetX(center, floor(XMVectorGetX(center)));
	center = XMVectorSetY(center, floor(XMVectorGetY(center)));
	center = XMVector3Transform(center, inverseLookAtMatrix);

	XMVECTOR lightPos = center + boundingBoxFrustum.Radius * normalizedLightDirection; //light vector points away from light source, therefore + 
	XMVECTOR lookAt = center;

	return XMMatrixLookAtLH(lightPos, lookAt, lightUp);
}

// The result must be transposed before being sent to shader
DirectX::XMMATRIX XM_CALLCONV ComputeTightOrthogonalViewProjectionMatrix(DirectX::FXMVECTOR lightDirection,
	const DirectX::BoundingBox& boundingBoxGeometry,
	const DirectX::BoundingSphere& boundingSphereFrustum,
	uint32_t textureSize)
{
	using namespace DirectX;

	XMMATRIX lightView = ComputeTightViewMatrix(lightDirection, boundingBoxGeometry, boundingSphereFrustum, textureSize);
	BoundingBox boundingBoxGeometryLS;
	boundingBoxGeometry.Transform(boundingBoxGeometryLS, lightView);

	auto [minGeometryLS, maxGeometryLS] = GetBoundingBoxMinMax(boundingBoxGeometryLS);

	const float radius = boundingSphereFrustum.Radius;
	XMMATRIX lightProjection = XMMatrixOrthographicOffCenterLH(-radius, radius, -radius, radius, minGeometryLS.z,
		maxGeometryLS.z);

	return lightView * lightProjection;
}

void UpdateLightDataCascade(std::span<const Light> lights,
	const DirectX::BoundingBox& boundingBoxGeometry,
	std::span<const DirectX::BoundingSphere> boundingSpheresFrusta,
	ShadowMaps& shadowMaps)
{
	using namespace DirectX;
	StackContext context;

	const uint32_t cascadeCount = static_cast<uint32_t>(boundingSpheresFrusta.size());
	const uint32_t lightsCount = static_cast<uint32_t>(lights.size());

	//CPU resident staging buffers in order to not work with gpu resident memory diectly
	ShadowedLight* activeLights = context.Allocate<ShadowedLight>(lightsCount);
	XMFLOAT4X4* transforms = context.Allocate<XMFLOAT4X4>(cascadeCount * lightsCount);

	for (uint32_t i = 0; i < lightsCount; i++)
	{
		static_cast<Light&>(activeLights[i]) = lights[i];

		activeLights[i].shadowDataOffsest = shadowMaps.transformsBuffer->Offset(i * cascadeCount);
		activeLights[i].shadowMapArrayIndex = i * cascadeCount;

		XMVECTOR lightDirection = XMLoadFloat3(&lights[i].direction);

		for (uint32_t j = 0; j < cascadeCount; j++)
		{
			BoundingSphere boundingBoxFrusta = boundingSpheresFrusta[j];
#if 1 //last level uses encompasses whole scene instead of just whole frustum
			if (j == cascadeCount - 1)
			{
				BoundingSphere::CreateFromBoundingBox(boundingBoxFrusta,boundingBoxGeometry);
			}
#endif
			XMMATRIX lightViewProjection = ComputeTightOrthogonalViewProjectionMatrix(lightDirection,
				boundingBoxGeometry,
				boundingBoxFrusta,
				shadowMaps.depthBuffer.properties.width);

			XMStoreFloat4x4(&transforms[j + i * cascadeCount], lightViewProjection);
		}
	}

	//upload data to GPU
	shadowMaps.lightsBuffer->Write({ activeLights, lightsCount});
	shadowMaps.transformsBuffer->Write({transforms, cascadeCount * lightsCount});
}

void UpdateShadowedPointLightsData(std::span<const Light> lights, ShadowMaps& shadowMaps)
{
	using namespace DirectX;
	StackContext context;
	const uint32_t lightsCount = static_cast<uint32_t>(lights.size());
	
	//CPU resident staging buffers in order to not work with gpu resident memory diectly
	ShadowedLight* activeLights = context.Allocate<ShadowedLight>(lightsCount);

	XMFLOAT4X4* transforms = context.Allocate<XMFLOAT4X4>(lightsCount * 6);

	for (uint32_t i = 0; i < lightsCount; i++)
	{
		const Light& light = lights[i];

		//prepare data for upload to GPU
		static_cast<Light&>(activeLights[i]) = lights[i];
		activeLights[i].shadowMapArrayIndex = i;

		for (uint32_t j = 0; j < 6; j++)
		{

			XMVECTOR lightPosition = XMLoadFloat3(&light.position);
			auto lightViewProjection = CalculateCubeMapViewProjection(j, lightPosition, omnidirectionalShadowMapNearZ, light.fadeEnd);

			XMStoreFloat4x4(&transforms[i * 6 + j], lightViewProjection);
		}
	}

	//upload data to GPU
	shadowMaps.lightsBuffer->Write({ activeLights, lightsCount});
	shadowMaps.transformsBuffer->Write({ transforms, lightsCount * 6 });
}

LightsData ComputeLightsData(ScratchHeap& bufferHeap,
	const Camera& camera,
	std::span<const Light> directionalLights,
	std::span<const Light> shadowedPointLights,
	std::span<const Light> pointLights,
	const DirectX::BoundingBox& boundingBoxGeometry,
	ShadowMaps& cascadedShadowMaps,
	ShadowMaps& pointLightShadowMaps)
{
	using namespace DirectX;

	const XMMATRIX cameraInverseView = XMLoadFloat4x4(&camera.constants->inverseViewMatrix);
	DirectX::BoundingSphere subFrustaBoundingSpheres[cascadeCount];
	DirectX::BoundingBox subFrustaBoundingBoxes[cascadeCount];
	ComputeWorldSpaceSubFrustaBoundingBoxes(cascadeSplits, camera, XMMatrixTranspose(cameraInverseView), subFrustaBoundingBoxes, subFrustaBoundingSpheres);

	TemporaryBuffer<BoundingBox> cascadeBoundingBoxBuffer = CreateTemporaryBuffer<BoundingBox>(bufferHeap, cascadeCount);
	cascadeBoundingBoxBuffer.Write(subFrustaBoundingBoxes);

	UpdateLightDataCascade(directionalLights, boundingBoxGeometry, subFrustaBoundingSpheres, cascadedShadowMaps);
	UpdateShadowedPointLightsData(shadowedPointLights, pointLightShadowMaps);

	BufferHeap::Offset pointsLightsBufferOffset = WriteTemporaryData(bufferHeap, pointLights);

	return
	{
		.pointLightsCount = static_cast<uint32_t>(pointLights.size()),
		.pointLightsBufferOffset = pointsLightsBufferOffset,
		.shadowedPointLightsCount = static_cast<uint32_t>(shadowedPointLights.size()),
		.shadowedPointLightsBufferOffset = pointLightShadowMaps.lightsBuffer->Offset(),
		.omnidirectionalShadowMapsSrvId = pointLightShadowMaps.depthBufferCubeArraySrvId,
		.directionalLightsCount = static_cast<uint32_t>(directionalLights.size()),
		.directionalLightsBufferOffset = cascadedShadowMaps.lightsBuffer->Offset(),
		.cascadedShadowMapsSrvId = cascadedShadowMaps.depthBuffer.srvId,
		.cascadeDataOffset = cascadeBoundingBoxBuffer.Offset(),
		.cascadeCount = cascadeCount,
	};
}

void ComputeWorldSpaceSubFrustaBoundingBoxes(const float(&splitRatios)[cascadeCount],
	const Camera& camera,
	DirectX::CXMMATRIX inverseView,
	DirectX::BoundingBox(&boundingBoxes)[cascadeCount],
	DirectX::BoundingSphere(&boundingSpheres)[cascadeCount])
{
	using namespace DirectX;

	camera.ComputeViewSpaceSubFrustaBoundingBoxes(splitRatios, boundingSpheres, boundingBoxes);
	for (auto& boundingBox : boundingBoxes)
	{
		boundingBox.Transform(boundingBox, inverseView);
	}
	for (auto& boundingSphere: boundingSpheres)
	{
		boundingSphere.Transform(boundingSphere, inverseView);
	}
}

DirectX::BoundingBox ComputeCompoundMeshBoundingBox(std::span<const PbrMesh*> meshList)
{
	DirectX::BoundingBox result;
	result.Extents = {};
	for (auto* mesh : meshList)
	{
		for (uint32_t i = 0; i < mesh->instanceCount; i++)
		{
			DirectX::XMMATRIX transform = DirectX::XMMatrixTranspose(DirectX::XMLoadFloat4x4(&mesh->GetInstanceData(i).transforms));
			DirectX::BoundingBox boundingBoxWS;
			mesh->geometry.aabb.Transform(boundingBoxWS, transform);
			DirectX::BoundingBox::CreateMerged(result, result, boundingBoxWS);
		}
	}
	
	return result;
}

void UI::LightingSettings::MenuEntry()
{
	if (ImGui::CollapsingHeader("Lighting Options", ImGuiTreeNodeFlags_None))
	{
		ImGui::Checkbox("Turn on specular", &useSpecular);
		ImGui::DragFloat("IBL Specular Intensity", &iblSpecularIntensity, 0.005f, 0.0f, FLT_MAX, "%.2f");
		ImGui::DragFloat("IBL Diffuse Intensity", &iblDiffuseIntensity, 0.005f, 0.0f, FLT_MAX, "%.2f");
		ImGui::DragFloat("Sky Brightness", &skyBrightness, 0.005f, 0.0f, FLT_MAX, "%.2f");
		ImGui::DragFloat("Shadow Darkness", &shadowDarkness, 0.005f, 0.0f, 1.0f, "%.2f");
	}
}

void UI::ShadowSettings::MenuEntry(LPCSTR name)
{
	ImGui::SeparatorText(name);
	char label[128];
	sprintf_s(label, "Light Depth Bias##%s", name); //@note: ## necessary such that sliders with the same name are independent
	ImGui::SliderInt(label, &depthBias, 0, 20000); 
	sprintf_s(label, "Slope Scaled Depth Bias##%s", name);
	ImGui::SliderFloat(label, &slopeScaledDepthBias, 0.0f, 100.0f, "%.1f");

	static const char* comboItems[] = { "None", "Front Face", "Back Face" };
	sprintf_s(label, "Cull Settings##%s", name);
	ImGui::Combo(label, reinterpret_cast<int*>(&cullSettings), comboItems, IM_ARRAYSIZE(comboItems));

	sprintf_s(label, "Apply##%s", name);
	if (ImGui::Button(label))
	{
		shadowSettingsApplied = true;
	}
	else
	{
		shadowSettingsApplied = false;
	}
}

void UpateShadowMaps(ShadowMaps& instance, const UI::ShadowSettings& settings, ID3D12Device10* device)
{
	static const RasterizerState rasterizerStateLookup[] = { RasterizerState::NoCull, RasterizerState::FrontFaceCull, RasterizerState::BackFaceCull };
	if (settings.shadowSettingsApplied)
	{
		RasterizerState rasterizerState = rasterizerStateLookup[settings.cullSettings];
		instance.RecompilePsoWithDepthBias(device, settings.depthBias, settings.slopeScaledDepthBias, rasterizerState);
	}
}
