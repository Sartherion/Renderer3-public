#include "stdafx.h"
#include "D3DDrawHelpers.h"

#include "Geometry.h"

static void InitPsos(ID3D12Device10* device, DXGI_FORMAT renderTargetFormat, DXGI_FORMAT depthBufferFormat);

namespace BasicShapes 
{
	Geometry sphere;
	Geometry cube;
}

namespace PsoType
{
	enum PsoType
	{
		Opaque,
		Skybox,
		None,
		Count = None
	};
}
static ComPtr<ID3D12PipelineState> psos[PsoType::Count];

namespace Draw
{
	void Opaque(ID3D12GraphicsCommandList10* commandList, ParametersOpaque parameters, std::span<const PbrMesh*> meshList, bool useDefaultPso)
	{
		if (useDefaultPso)
		{
			commandList->SetPipelineState(psos[PsoType::Opaque].Get());
		}

		BindGraphicsRootConstants<4>(commandList,
			parameters.ssaoBufferSrvId,
			parameters.sssrBufferSrvId,
			parameters.indirectDiffuseBufferSrvId);

		for (const auto* mesh : meshList)
		{
			mesh->Draw(commandList);
		}
	}

	void SkyBox(ID3D12GraphicsCommandList10* commandList, DescriptorHeap::Id skyBoxTextureSrvId)
	{
		commandList->SetPipelineState(psos[PsoType::Skybox].Get());
		commandList->SetGraphicsRoot32BitConstant(0, skyBoxTextureSrvId, 0);
		BasicShapes::sphere.Draw(commandList);
	}
}

void InitDrawHelpers(ID3D12Device10* device, BufferHeap& bufferHeap, DXGI_FORMAT renderTargetFormat, DXGI_FORMAT depthBufferFormat)
{
	InitPsos(device, renderTargetFormat, depthBufferFormat);

	BasicShapes::sphere = LoadGeometryData(bufferHeap, L"content\\geometry\\sphere.obj");
	BasicShapes::cube = LoadGeometryData(bufferHeap, L"content\\geometry\\cube3.obj");
}

void BindFixedRenderGraphicsRootConstants(ID3D12GraphicsCommandList10* commandList,
	BufferHeap::Offset lightingDataBufferOffset,
	BufferHeap::Offset cameraConstantsBufferOffset,
	BufferHeap::Offset dimensionsBufferOffset,
	RenderFeatures renderFeatures)
{
	BindGraphicsRootConstants<7>(commandList,
		lightingDataBufferOffset,
		renderFeatures.asUint,
		cameraConstantsBufferOffset,
		dimensionsBufferOffset);
}


void InitPsos(ID3D12Device10* device, DXGI_FORMAT renderTargetFormat, DXGI_FORMAT depthBufferFormat) 
{
	psos[PsoType::Opaque] = CreateGraphicsPso(device,
		{
			.vs = LoadShaderBinary(L"content\\shaderbinaries\\BasicVS.cso").Get(),
			.ps = LoadShaderBinary(L"content\\shaderbinaries\\BasicPS.cso").Get(),
			.depthState = GetDepthState(DepthState::Write),
			.rtvFormats = {{ renderTargetFormat }},
			.dsvFormat = depthBufferFormat
		}
	);

	D3D12_DEPTH_STENCIL_DESC depthState = GetDepthState(DepthState::TestEqual);
	depthState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	psos[PsoType::Skybox] = CreateGraphicsPso(device,
		{
			.vs = LoadShaderBinary(L"content\\shaderbinaries\\SkyboxVS.cso").Get(),
			.ps = LoadShaderBinary(L"content\\shaderbinaries\\SkyboxPS.cso").Get(),
			.rasterizerState = GetRasterizerState(RasterizerState::FrontFaceCull),
			.depthState = depthState,
			.rtvFormats = {{ renderTargetFormat }},
			.dsvFormat = depthBufferFormat
		}
	);
}
