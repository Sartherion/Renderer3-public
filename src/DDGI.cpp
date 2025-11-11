#include "stdafx.h"
#include "DDGI.h"

#include "D3DDrawHelpers.h"
#include "D3DGlobals.h"
#include "Geometry.h"
#include "Texture.h"

namespace DDGI
{
	struct AtlasData
	{
		DescriptorHeap::Id uavId;
		DescriptorHeap::Id srvId;
		uint32_t tileSize;
		uint32_t dimensionsX;
		uint32_t dimensionsY;
		uint32_t elementsPerRow;
	};

	struct ProbeGridData
	{
		DirectX::XMUINT3 gridDimensions;
		DirectX::XMFLOAT3 probeSpacing;
		DirectX::XMFLOAT3 gridCenter;
		DirectX::XMFLOAT3 gridOrigin;
		uint32_t probeCount;
	};

	struct GpuData
	{
		AtlasData depthAtlasData;
		AtlasData irradianceAtlasData;
		AtlasData radianceAtlasData;
		ProbeGridData probeGridData;
	};

	static uint32_t probeCount;

	static RWTexture depthAtlas;
	static RWTexture radianceAtlas;
	static RWTexture irradianceAtlas;

	static PersistentBuffer<GpuData> dataBuffer;

	static ComPtr<ID3D12PipelineState> rayTracingPso;
	static ComPtr<ID3D12PipelineState> copyBorderPixelsPso;
	static ComPtr<ID3D12PipelineState> debugVisualizationPso;

	static void InitPsos(ID3D12Device10* device);

	void Init(ID3D12Device10* device, DescriptorHeap& descriptorHeap, BufferHeap& bufferHeap, InitData&& initData)
	{
		InitPsos(device);

		probeCount = initData.gridDimensions.x * initData.gridDimensions.y * initData.gridDimensions.z;

		using namespace DirectX;
		XMFLOAT3 ddgiGridOrigin = { 0.0f, 0.0f, 0.0f };

		XMVECTOR probeSpacing = XMLoadFloat3(&initData.probeSpacing);
		XMVECTOR gridDimensions = XMLoadUInt3(&initData.gridDimensions);
		gridDimensions -= XMVectorSet(1, 1, 1, 0);
		XMVECTOR ddgiGridHalfExtent = gridDimensions * probeSpacing / 2;
		XMVECTOR gridCenter = XMLoadFloat3(&ddgiGridOrigin) + ddgiGridHalfExtent;

		gridCenter += XMLoadFloat3(&initData.relativeOffset) * 2.0f * ddgiGridHalfExtent;

		XMFLOAT3 ddgiGridCenter;
		DirectX::XMStoreFloat3(&ddgiGridCenter, gridCenter);

		XMVECTOR origin = gridCenter - ddgiGridHalfExtent;
		XMStoreFloat3(&ddgiGridOrigin, origin);

		dataBuffer = CreatePersistentBuffer<GpuData>(bufferHeap);

		depthAtlas = CreateRWTexture(device,
			{ .format = DXGI_FORMAT_R16G16_FLOAT, .width = initData.depthAtlasDimensionX, .height = initData.depthAtlasDimensionY },
			descriptorHeap,
			L"DDGI Depth Atlas");

		radianceAtlas = CreateRWTexture(device,
			{ .format = DXGI_FORMAT_R16G16B16A16_FLOAT, .width = initData.radianceAtlasDimensionX, .height = initData.radianceAtlasDimensionY }, //@note: r11g11b10 does not allow negative values
			descriptorHeap,
			L"DDGI Radiance Atlas");

		irradianceAtlas = CreateRWTexture(device,
			{ .format = DXGI_FORMAT_R16G16B16A16_FLOAT, .width = initData.irradianceAtlasDimensionX, .height = initData.irradianceAtlasDimensionY }, 
			descriptorHeap,
			L"DDGI Irradiance Atlas");

		const uint32_t depthAtlasElementsPerRowCount = initData.depthAtlasDimensionX / initData.depthAtlasTileSize;
		const uint32_t radianceAtlasElementsPerRowCount = initData.radianceAtlasDimensionX / initData.radianceAtlasTileSize;
		const uint32_t irradianceAtlasElementsPerRowCount = initData.irradianceAtlasDimensionX / initData.irradianceAtlasTileSize;

		GpuData gpuData =
		{
			.depthAtlasData =
			{
				.uavId = depthAtlas.uavId,
				.srvId = depthAtlas.srvId,
				.tileSize = initData.depthAtlasTileSize,
				.dimensionsX = initData.depthAtlasDimensionX,
				.dimensionsY = initData.depthAtlasDimensionY,
				.elementsPerRow = depthAtlasElementsPerRowCount,
			},
			.irradianceAtlasData =
			{
				.uavId = irradianceAtlas.uavId,
				.srvId = irradianceAtlas.srvId,
				.tileSize = initData.irradianceAtlasTileSize,
				.dimensionsX = initData.irradianceAtlasDimensionX,
				.dimensionsY = initData.irradianceAtlasDimensionY,
				.elementsPerRow = irradianceAtlasElementsPerRowCount,
			},
			.radianceAtlasData =
			{
				.uavId = radianceAtlas.uavId,
				.srvId = radianceAtlas.srvId,
				.tileSize = initData.radianceAtlasTileSize,
				.dimensionsX = initData.radianceAtlasDimensionX,
				.dimensionsY = initData.radianceAtlasDimensionY,
				.elementsPerRow = radianceAtlasElementsPerRowCount,
			},
			.probeGridData =
			{
				.gridDimensions = initData.gridDimensions,
				.probeSpacing = initData.probeSpacing,
				.gridCenter = ddgiGridCenter,
				.gridOrigin = ddgiGridOrigin,
				.probeCount = probeCount

			}
		};

		dataBuffer.Write(gpuData);

		bufferOffset = dataBuffer.Offset();

		{
			const uint32_t depthAtlasElementsMaxCount = depthAtlasElementsPerRowCount * initData.depthAtlasDimensionY / initData.depthAtlasTileSize;
			const uint32_t radianceAtlasElementsMaxCount = radianceAtlasElementsPerRowCount * initData.radianceAtlasDimensionY / initData.radianceAtlasTileSize;
			const uint32_t irradianceAtlasElementsMaxCount = irradianceAtlasElementsPerRowCount * initData.irradianceAtlasDimensionY / initData.irradianceAtlasTileSize;
			assert(probeCount <= depthAtlasElementsMaxCount);
			assert(probeCount <= radianceAtlasElementsMaxCount);
			assert(probeCount <= irradianceAtlasElementsMaxCount);
		}
	}
	
	void Render(ID3D12GraphicsCommandList10* commandList, BufferHeap::Offset lightingDataOffset, const TlasData& tlasData, DescriptorHeap::Id skySrvId)
	{
		PIXScopedEvent(commandList, PIX_COLOR_DEFAULT, "DDGI");
		// raytracing and shading pass
		{
			DispatchComputePass(commandList,
				rayTracingPso.Get(),
				{
					dataBuffer.Offset(),
					tlasData.accelerationStructureSrvId,
					skySrvId,
					tlasData.instanceGeometryDataOffset,
					lightingDataOffset
				},
				{ .dispatchX = 1, .dispatchY = 1, .dispatchZ = probeCount });

			ResourceTransitions(commandList,
				{
					BarrierCSWriteToWrite(depthAtlas),
					BarrierCSWriteToWrite(radianceAtlas),
					BarrierCSWriteToWrite(irradianceAtlas)
				});
		}

		// copy border pass
		{
			commandList->SetPipelineState(copyBorderPixelsPso.Get());

			commandList->Dispatch(1, 1, probeCount);

			ResourceTransitions(commandList,
				{
					depthAtlas.Barrier(ResourceState::WriteCS, ResourceState::ReadAny),
					radianceAtlas.Barrier(ResourceState::WriteCS, ResourceState::ReadAny),
					irradianceAtlas.Barrier(ResourceState::WriteCS, ResourceState::ReadAny),
				});
		}

	}

	void DrawDebugVisualization(ID3D12GraphicsCommandList10* commandList)
	{
		PIXScopedEvent(commandList, PIX_COLOR_DEFAULT, "DDGI Visualization");
		commandList->SetPipelineState(debugVisualizationPso.Get());
		BindGraphicsRootConstants(commandList, dataBuffer.Offset());

		BasicShapes::sphere.Bind(commandList);
		BasicShapes::sphere.Draw(commandList, probeCount);
	}

	void FrameEnd(ID3D12GraphicsCommandList10* commandList)
	{
		ResourceTransitions(commandList,
			{
				depthAtlas.Done(ResourceState::ReadAny, D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_UNORDERED_ACCESS),
				radianceAtlas.Done(ResourceState::ReadAny, D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_UNORDERED_ACCESS),
				irradianceAtlas.Done(ResourceState::ReadAny, D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_UNORDERED_ACCESS)
			});
	}

	void InitPsos(ID3D12Device10* device)
	{
		rayTracingPso = CreateComputePso(device, { .cs = LoadShaderBinary(L"content\\shaderbinaries\\DDGIRayTracingCS.cso").Get() });
		copyBorderPixelsPso = CreateComputePso(device, { .cs = LoadShaderBinary(L"content\\shaderbinaries\\DDGICopyBorderPixelsCS.cso").Get() });
		debugVisualizationPso = CreateGraphicsPso(device, { 
			.vs = LoadShaderBinary(L"content\\shaderbinaries\\DDGIVisualizationVS.cso").Get(),
			.ps = LoadShaderBinary(L"content\\shaderbinaries\\DDGIVisualizationPS.cso").Get(),
			.depthState = GetDepthState(DepthState::Test),
			.rtvFormats = {{D3D::HDRRenderTargetFormat}},
			.dsvFormat = D3D::depthStencilFormat,
			});
	}
}

void UI::DDGISettings::MenuEntry()
{
	if (ImGui::CollapsingHeader("DDGI Options", ImGuiTreeNodeFlags_None))
	{
		ImGui::DragFloat("Indirect Diffuse Bounce Intensity", &higherBounceIndirectDiffuseIntensity, 0.005f, 0.0f, 1.0f, "%.01f");
		ImGui::DragFloat("Indirect Specular Bounce Intensity", &higherBounceIndirectSpecularIntensity, 0.005f, 0.0f, 1.0f, "%.01f");
		ImGui::DragFloat("Irradiance Gamma Exponent", &irradianceGammaExponent, 0.005f, 1.0f, 10.0f, "%.01f");
		ImGui::DragFloat("History Blend Weight", &historyBlendWeight, 0.005f, 0.0f, 1.0f, "%.3f");
	}
}
