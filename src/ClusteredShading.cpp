#include "stdafx.h"
#include "ClusteredShading.h"

#include "D3DDrawHelpers.h"
#include "Geometry.h"
#include "Light.h"
#include "SharedDefines.h"

using LightNode = uint64_t;

enum LightType : uint32_t
{
	Point,
	PointShadowed,
	Count
};

static LPCSTR lightTypeNames[LightType::Count] = 
{
	"Point Lights",
	"Shadowed Point Lights"
};

static const DXGI_FORMAT shellRTFormat = DXGI_FORMAT_R8G8_UNORM;
static const float clearColor[4] = { 1.0f, 1.0f, 0.0f, 0.0f };
static ComPtr<ID3D12PipelineState> shellPassPsos[LightType::Count];
static ComPtr<ID3D12PipelineState> fillPassPso;

static void InitializePsos(ID3D12Device10* device);

void ClusteredShadingContext::Init(ID3D12Device10* device,
	DescriptorHeap& descriptorHeap,
	BufferHeap& bufferHeap,
	uint32_t renderTargetWidth,
	uint32_t renderTargetHeight,
	LPCWSTR name,
	uint32_t maximumLightsPerShellPass)
{
	clusterCountX = DivisionRoundUp(renderTargetWidth, clusteredShadingTileSizeX);
	clusterCountY = DivisionRoundUp(renderTargetHeight, clusteredShadingTileSizeY);
	clusterCount = clusterCountX * clusterCountY * clusterCountZ;

	maxLightsPerShellPass = maximumLightsPerShellPass; //@note: this limit could be lifted by using more than one pass per light type
	maxLightsCount = maxLightsPerShellPass;
	maxLightNodeCount = clusterCount * 200;

	D3D12_CLEAR_VALUE clearValue = { .Format = shellRTFormat, .Color = {clearColor[0], clearColor[1], clearColor[2], clearColor[3]} };
	lightShellRT = CreateRenderTarget(device,
		{ .format = shellRTFormat, .width = clusterCountX, .height = clusterCountY, .arraySize = maxLightsCount },
		descriptorHeap,
		L"LightShellRT",
		TextureUsage::Default,
		TextureMemoryType::Default,
		D3D12_BARRIER_LAYOUT_RENDER_TARGET,
		&clearValue);

	InitializePsos(device);

	lightsHeadNodesBuffer = CreateRWRawBuffer(device, { .size = clusterCount * sizeof(uint32_t), .name = L"LightsHeadNodesBuffer" }, descriptorHeap, D3D12_HEAP_TYPE_DEFAULT);

	linkedLightListBuffer = CreateRWRawBufferWithCounter(device, { .size = sizeof(LightNode) * maxLightNodeCount, .name = L"LinkedLightListNodeBuffer" }, descriptorHeap, D3D12_HEAP_TYPE_DEFAULT);

	clusterDataBuffer = CreatePersistentBuffer<ClusterData>(bufferHeap);
	clusterDataBuffer.Write({
			.clusteredLightListHeadNodesId = lightsHeadNodesBuffer.srvId,
			.clusteredLinkedLightListId = linkedLightListBuffer.srvId,
			.clusterCountX = clusterCountX,
			.clusterCountY = clusterCountY,
			.clusterCountZ = clusterCountZ 
		});

	wcscpy_s(this->name, _countof(this->name), name);
}

void ClusteredShadingContext::RenderShellPass(ID3D12GraphicsCommandList10* commandList,
	LightType lightType,
	BufferHeap::Offset lightsDataOffset,
	uint32_t lightsCount,
	BufferHeap::Offset cameraDataOffset)
{
	static const Geometry* const lightTypeShapes[LightType::Count] =
	{
		&BasicShapes::sphere,
		&BasicShapes::sphere
	};

	PIXScopedEvent(commandList, PIX_COLOR_DEFAULT, "Shell Pass");

	if (lightsCount > 0)
	{
		commandList->SetPipelineState(shellPassPsos[lightType].Get());

		lightShellRT.Bind(commandList);
		lightShellRT.Clear(commandList, uint32_t(-1), clearColor);

		BindGraphicsRootConstants<4>(commandList,
				lightsDataOffset,
				clusterCountX,
				clusterCountY,
				cameraDataOffset);
		lightTypeShapes[lightType]->Draw(commandList, lightsCount);
	}

	ResourceTransitions(commandList, { lightShellRT.Barrier(ResourceState::RenderTarget, ResourceState::ReadAny) });
}

void ClusteredShadingContext::FillPass(ID3D12GraphicsCommandList10* commandList,
	TemporaryDescriptorHeap& descriptorHeap,
	LightType lightType,
	uint32_t lightsCount,
	BufferHeap::Offset cameraDataOffset,
	const RWTexture* depthPyramide)
{
	DescriptorHeap::Id depthSrvId = DescriptorHeap::InvalidId;
	if (depthPyramide)
	{
		const uint32_t relevantMipLevel = Max(GetMostSignificantBitPosition(clusteredShadingTileSizeX),
			GetMostSignificantBitPosition(clusteredShadingTileSizeY)); // choose mip level such that its pixels have at least tile size
		depthSrvId = CreateSrvOnHeap(descriptorHeap, depthPyramide->GetSubresource({ .mipRange = {.begin = relevantMipLevel } }));
	}

	if (lightsCount > 0)
	{
		DispatchComputePass(commandList,
			fillPassPso.Get(),
			{
				lightShellRT.srvId,
				depthSrvId,
				lightsHeadNodesBuffer.uavId,
				linkedLightListBuffer.uavId,
				static_cast<uint32_t>(lightType),
				clusterCountX,
				clusterCountY,
				cameraDataOffset

			},
			{ .dispatchX = clusterCountX, .dispatchY = clusterCountY, .dispatchZ = lightsCount },
			L"FillPass");
	}

	ResourceTransitions(commandList, 
		{
			linkedLightListBuffer.Barrier(ResourceState::WriteCS, ResourceState::ReadAny),
			lightsHeadNodesBuffer.Barrier(ResourceState::WriteCS, ResourceState::ReadAny)
		},
		{ lightShellRT.Barrier(ResourceState::ReadAny, ResourceState::RenderTarget) }
	);
}

void ClusteredShadingContext::PrepareClusterData(ID3D12GraphicsCommandList10* commandList,
	TemporaryDescriptorHeap& descriptorHeap,
	const LightsData& lightsData,
	BufferHeap::Offset cameraDataOffset,
	const RWTexture* depthPyramide)
{
	PIXScopedEvent(commandList, PIX_COLOR_DEFAULT, "Cluster Generation: %ls", name);
	{
		PIXScopedEvent(commandList, PIX_COLOR_DEFAULT, "Clear Data");
		ClearBufferUav(commandList, lightsHeadNodesBuffer.uavId, uint32_t(-1), clusterCount);

		linkedLightListBuffer.ResetCounter(commandList);

		ResourceTransitions(commandList, {
			linkedLightListBuffer.Barrier(ResourceState::CopyDestination, ResourceState::WriteCS),
			lightsHeadNodesBuffer.Barrier(ResourceState::WriteCS, ResourceState::WriteCS),
			}
		);
	}

	{
		LightType lightType = LightType::Point;
		PIXScopedEvent(commandList, PIX_COLOR_DEFAULT, "%s", lightTypeNames[lightType]);
		RenderShellPass(commandList, lightType, lightsData.pointLightsBufferOffset, lightsData.pointLightsCount, cameraDataOffset);
		FillPass(commandList, descriptorHeap, lightType, lightsData.pointLightsCount, cameraDataOffset, depthPyramide);
	}

	{
		LightType lightType = LightType::PointShadowed;
		PIXScopedEvent(commandList, PIX_COLOR_DEFAULT, "%s", lightTypeNames[lightType]);
		RenderShellPass(commandList, lightType, lightsData.shadowedPointLightsBufferOffset, lightsData.shadowedPointLightsCount, cameraDataOffset);
		FillPass(commandList, descriptorHeap, lightType, lightsData.shadowedPointLightsCount, cameraDataOffset, depthPyramide);
	}
}

void ClusteredShadingContext::Free()
{
	DestroySafe(lightShellRT);

	DestroySafe(linkedLightListBuffer);
	DestroySafe(lightsHeadNodesBuffer);

	Frame::SafeRelease(clusterDataBuffer);
}

static void InitializePsos(ID3D12Device10* device)
{
	D3D12_RASTERIZER_DESC rasterizerDesc = GetRasterizerState(RasterizerState::NoCull);
	rasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_ON;

	D3D12_BLEND_DESC blendDesc = 
	{
		.RenderTarget = 
		{
			{
				.BlendEnable = true,
				.SrcBlend = D3D12_BLEND_ONE,
				.DestBlend = D3D12_BLEND_ONE,
				.BlendOp = D3D12_BLEND_OP_MIN,
				.SrcBlendAlpha = D3D12_BLEND_ZERO,
				.DestBlendAlpha = D3D12_BLEND_ZERO,
				.BlendOpAlpha = D3D12_BLEND_OP_ADD,
				.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_RED | D3D12_COLOR_WRITE_ENABLE_GREEN,
			}
		}
	};
	shellPassPsos[LightType::Point] = CreateGraphicsPso(device,
		{
			.vs = LoadShaderBinary(L"content\\shaderbinaries\\ClusteredPointLightShellVS.cso").Get(),
			.ps = LoadShaderBinary(L"content\\shaderbinaries\\ClusteredShellPS.cso").Get(),
			.rasterizerState = rasterizerDesc,
			.blendState = blendDesc,
			.depthState = GetDepthState(DepthState::Disabled),
			.rtvFormats = {{shellRTFormat}}
		});
	shellPassPsos[LightType::PointShadowed] = CreateGraphicsPso(device,
		{
			.vs = LoadShaderBinary(L"content\\shaderbinaries\\ClusteredShadowedPointLightShellVS.cso").Get(),
			.ps = LoadShaderBinary(L"content\\shaderbinaries\\ClusteredShellPS.cso").Get(),
			.rasterizerState = rasterizerDesc,
			.blendState = blendDesc,
			.depthState = GetDepthState(DepthState::Disabled),
			.rtvFormats = {{shellRTFormat}}
		});

	fillPassPso = CreateComputePso(device, { .cs = LoadShaderBinary(L"content\\shaderbinaries\\FillPassCS.cso").Get() });
}