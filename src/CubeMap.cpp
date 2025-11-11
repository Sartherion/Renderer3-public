#include "stdafx.h"
#include "CubeMap.h"

#include "ClusteredShading.h"
#include "D3DDrawHelpers.h"
#include "MipGeneration.h"


static DirectX::XMMATRIX XM_CALLCONV BuildCubeMapViewMatrix(DirectX::FXMVECTOR position, int axis);

	static ClusteredShadingContext cubeMapClusteredShadingContext; 

void CubeMaps::Init(ID3D12Device10* device,
	uint32_t size,
	uint32_t cubeMapCount,
	uint32_t mipCount,
	DXGI_FORMAT renderFormat,
	DXGI_FORMAT depthFormat,
	DescriptorHeap& descriptorHeap,
	BufferHeap& bufferHeap)
{
	const uint32_t arraySize = 6 * cubeMapCount;

	//prepare render target array
	renderTargets = CreateRenderTarget(device,
		{ .format = renderFormat, .width = size, .height = size, .arraySize = arraySize, .mipCount = mipCount },
		descriptorHeap,
		L"CubeMapRenderTargets",
		TextureUsage::ReadWrite,
		TextureMemoryType::Default,
		D3D12_BARRIER_LAYOUT_RENDER_TARGET,
		nullptr,
		TextureArrayType::CubeMapArray);

	//prepare depth buffer. No array is needed, because it can be reused for each cubemap face
	depthBuffer = CreateDepthBuffer(device,
		{ .format = depthFormat, .width = size, .height = size },
		descriptorHeap,
		L"CubeMapDepthBuffer",
		TextureUsage::DisallowShader); //no need to read from depth buffers in shaders

	downsamplingTemporaryTexture = CreateRWTexture(device,
		{ .format = renderFormat, .width = size, .height = size, .mipCount = mipCount },
		descriptorHeap,
		L"CubeMapDownsampleTemporary");

	cubeMapClusteredShadingContext.Init(device, descriptorHeap, bufferHeap, size, size, L"CubeMaps");

	const Texture2DDimensions renderTargetDimensions = GetTexture2DDimensions(renderTargets);
	dimensionsBuffer = CreatePersistentBuffer<Texture2DDimensions>(bufferHeap); 
	dimensionsBuffer.Write(renderTargetDimensions);

	perFrameFaceUpdatesCount = arraySize;
	lastUpdatedFace = 0;
}

void CubeMaps::Free()
{
	DestroySafe(renderTargets);
	DestroySafe(depthBuffer);
	DestroySafe(downsamplingTemporaryTexture);
	Frame::SafeRelease(dimensionsBuffer);
}

void CubeMaps::RenderBegin(ID3D12GraphicsCommandList10* commandList, ScratchHeap& bufferHeap, const LightingData& lightingData)
{
	LightingData cubeMapsLightingData = lightingData;
	cubeMapsLightingData.cubeMapSpecularId = DescriptorHeap::InvalidId;
	cubeMapsLightingData.clusterDataOffset = cubeMapClusteredShadingContext.GetClusterDataBufferOffset();

	lightingDataBuffer = CreateTemporaryBuffer<LightingData>(bufferHeap);
	lightingDataBuffer.Write(cubeMapsLightingData);

	PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, "CubeMapPasses");

	if (perFrameFaceUpdatesCount == renderTargets.properties.arraySize)
	{
		renderTargets.Clear(commandList);
	}
}

void CubeMaps::PassBegin(ID3D12GraphicsCommandList10* commandList,
	TemporaryDescriptorHeap& temporaryDescriptorHeap,
	const LightingData& lightingData,
	BufferHeap::Offset cameraDataBaseOffset,
	uint32_t index) const
{
	PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, "Pass %d", index);
	BufferHeap::Offset cameraDataOffset = cameraDataBaseOffset + index * sizeof(Camera::Constants);
	cubeMapClusteredShadingContext.PrepareClusterData(commandList, temporaryDescriptorHeap, lightingData.lightsData, cameraDataOffset);

	BindFixedRenderGraphicsRootConstants(commandList,
		lightingDataBuffer.Offset(),
		cameraDataOffset,
		dimensionsBuffer.Offset(),
		{ .asBitfield = {/*.sampleSpecularDDGI = false,*/ .forceLastShadowCascade = true } });


	PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, "Drawcalls");
	if (perFrameFaceUpdatesCount != renderTargets.properties.arraySize)
	{
		renderTargets.Clear(commandList, index);
	}
	depthBuffer.Clear(commandList);

	Bind(commandList, { {renderTargets} }, depthBuffer, { {index} }, 0);
}

void CubeMaps::PassEnd(ID3D12GraphicsCommandList10* commandList, uint32_t index) const
{
	PIXEndEvent(commandList);
	PIXEndEvent(commandList);
}

void CubeMaps::RenderEnd(ID3D12GraphicsCommandList10* commandList, TemporaryDescriptorHeap& temporaryDescriptorHeap, uint32_t activeCubeMapsFaceCount)
{
	ResourceTransitions(commandList, { renderTargets.Barrier(ResourceState::RenderTarget, ResourceState::ReadCS) });

	PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, "CubeMapDownSampling");
	{
		if (activeCubeMapsFaceCount > 0)
		{
			for (uint32_t arrayIndex = lastUpdatedFace; arrayIndex < static_cast<uint32_t>(lastUpdatedFace + perFrameFaceUpdatesCount); arrayIndex++)
			{
				uint32_t wrappedArrayIndex = arrayIndex % activeCubeMapsFaceCount;
				TextureSubresource subresource = renderTargets.GetSubresource({ .mipRange = Range::Full, .arrayRange = {.begin = wrappedArrayIndex } });

				wchar_t passName[32];
				swprintf(passName, sizeof(passName), L"Pass %d", wrappedArrayIndex);

				MipGenerator::GenerateMipsSeparableKernel(commandList,
					temporaryDescriptorHeap,
					subresource,
					MipGenerator::SeparableKernel::Gauss3x3,
					downsamplingTemporaryTexture,
					passName);
			}
		}
	}
	PIXEndEvent(commandList);

	PIXEndEvent(commandList); //EndEvent from RenderBegin()
}

D3D12_TEXTURE_BARRIER CubeMaps::Done() const
{
	return renderTargets.Done(ResourceState::ReadPS, D3D12_BARRIER_LAYOUT_RENDER_TARGET);
}

void UpdateCubeMapCameraData(BufferHeap& bufferHeap,
	BufferHeap::Offset cameraDataOffset,
	const DirectX::XMFLOAT3& cubeMapPosition,
	float nearPlane,
	float farPlane)
{
	using namespace DirectX;
	Camera::Constants constants[6];

	XMVECTOR position = XMLoadFloat3(&cubeMapPosition);
	
	for (int i = 0; i < 6; i++)
	{
		auto viewMatrix = BuildCubeMapViewMatrix(position, i);
		auto projectionMatrix = XMMatrixPerspectiveFovLH(XM_PI / 2, 1.0f, nearPlane, farPlane);
		auto viewProjectionMatrix = XMMatrixMultiply(viewMatrix, projectionMatrix);
		XMStoreFloat4x4(&constants[i].viewProjectionMatrix, XMMatrixTranspose(viewProjectionMatrix));
		XMStoreFloat4x4(&constants[i].viewMatrix, XMMatrixTranspose(viewMatrix));
		XMStoreFloat4x4(&constants[i].projectionMatrix, XMMatrixTranspose(projectionMatrix));
		XMStoreFloat3(&constants[i].cameraPosition, position);

		constants[i].frustumData = {
			.nearZ = nearPlane,
			.farZ = farPlane,
			//@todo: calculate other data if needed
		};
	}

	bufferHeap.WriteRaw(cameraDataOffset, constants, sizeof(constants));
}

DirectX::XMMATRIX XM_CALLCONV BuildCubeMapViewMatrix(DirectX::FXMVECTOR position, int axis)
{
	using namespace DirectX;
	static const XMVECTORF32
		x = { 1.0f, 0.0f, 0.0f, 0.0f },
		y = { 0.0f, 1.0f, 0.0f, 0.0f },
		z = { 0.0f, 0.0f, 1.0f, 0.0f };

	static const XMVECTOR bases[6][3] =
	{
		{ -z, y, x },
		{ z, y, -x },
		{ x, -z, y },
		{ x, z, -y },
		{ x, y, z },
		{ -x, y, -z }
	};


    XMVECTOR v = bases[axis][1];
    XMVECTOR w = bases[axis][2];

	return XMMatrixLookToLH(position, w, v);
}

DirectX::XMMATRIX XM_CALLCONV CalculateCubeMapViewProjection(int planeIndex, const DirectX::FXMVECTOR position, float nearPlane, float farPlane)
{
	using namespace DirectX;

	return XMMatrixMultiply(BuildCubeMapViewMatrix(position, planeIndex), XMMatrixPerspectiveFovLH(XM_PI / 2, 1.0f, nearPlane, farPlane));
}
