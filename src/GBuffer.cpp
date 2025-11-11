#include "stdafx.h"
#include "GBuffer.h"

#include "BufferMemory.h"
#include "D3DDrawHelpers.h"
#include "DepthBuffer.h"
#include "MipGeneration.h"
#include "RenderTarget.h"
#include "SharedDefines.h"

namespace GBuffer
{
	PingPong<RenderTarget> albedoRT;
	PingPong<RenderTarget> normalsRT;
	RenderTarget velocitiesRT;
	PingPong<DepthBuffer> depthBuffer;
	RWTexture depthPyramide;


	static const DXGI_FORMAT normalsRTFormat = DXGI_FORMAT_R16G16B16A16_UNORM;
	static const DXGI_FORMAT albedoRTFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	static const DXGI_FORMAT velocityRTFormat = DXGI_FORMAT_R16G16_FLOAT;
	static DXGI_FORMAT depthBufferFormat;

	static ComPtr<ID3D12PipelineState> gBufferPso;
	static ComPtr<ID3D12PipelineState> copyDepthToDepthPyramidePso;
	static ComPtr<ID3D12PipelineState> deferredShadingPso;

	void Init(ID3D12Device10* device,
		uint32_t renderTargetWidth,
		uint32_t renderTargetHeight,
		DXGI_FORMAT depthBufferFormat,
		DescriptorHeap& descriptorHeap,
		BufferHeap& bufferHeap)
	{
		GBuffer::depthBufferFormat = depthBufferFormat;
		
		albedoRT = CreatePingPongRenderTarget(device,
			{ .format = albedoRTFormat, .width = renderTargetWidth, .height = renderTargetHeight },
			descriptorHeap,
			L"AlbedoRT");

		normalsRT = CreatePingPongRenderTarget(device,
			{ .format = normalsRTFormat, .width = renderTargetWidth, .height = renderTargetHeight },
			descriptorHeap,
			L"NormalsRT");

		velocitiesRT = CreateRenderTarget(
			device,
			{ .format = velocityRTFormat, .width = renderTargetWidth, .height = renderTargetHeight },
			descriptorHeap,
			L"VelocitiesRT");

		depthBuffer = CreatePingPongDepthBuffer(device,
			{ .format = depthBufferFormat, .width = renderTargetWidth, .height = renderTargetHeight },
			descriptorHeap,
			L"DepthBuffer0");

		uint32_t depthPyramideLevelsCount = Max(GetMostSignificantBitPosition(renderTargetWidth) + 1, GetMostSignificantBitPosition(renderTargetHeight) + 1);

		depthPyramide = CreateRWTexture(device,
			{ .format = DXGI_FORMAT_R32G32_FLOAT, .width = renderTargetWidth, .height = renderTargetHeight, .mipCount = depthPyramideLevelsCount },
			descriptorHeap,
			L"Depth Pyramide",
			TextureMemoryType::Default,
			D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_UNORDERED_ACCESS
		);

		gBufferPso = CreateGraphicsPso(device,
			{
				.vs = LoadShaderBinary(L"content\\shaderbinaries\\BasicVS.cso").Get(),
				.ps = LoadShaderBinary(L"content\\shaderbinaries\\GBufferPS.cso").Get(),
				.depthState = GetDepthState(DepthState::Write),
				.rtvFormats = {{albedoRTFormat, normalsRTFormat, velocityRTFormat}},
				.dsvFormat = depthBufferFormat,
			});

		deferredShadingPso = CreateComputePso(device, { .cs = LoadShaderBinary(L"content\\shaderbinaries\\GBufferLightingCS.cso").Get() });

		copyDepthToDepthPyramidePso = CreateComputePso(device, { .cs = LoadShaderBinary(L"content\\shaderbinaries\\CopyDepthToDepthPyramideCS.cso").Get() });
	}

	void RenderBegin(ID3D12GraphicsCommandList10* commandList, BufferHeap::Offset cameraDataOffset)
	{
		PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, "GBuffer Generation");
		PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, "GBuffer Drawcalls");

		//Clear Render targets / Depth Buffer and bind them
		albedoRT.Current().Clear(commandList);
		normalsRT->Clear(commandList);
		velocitiesRT.Clear(commandList);
		depthBuffer->Clear(commandList);
		Bind(commandList, { { albedoRT.Current(), normalsRT, velocitiesRT} }, depthBuffer);

		commandList->SetPipelineState(gBufferPso.Get());
		commandList->SetGraphicsRoot32BitConstant(0, cameraDataOffset, 9);
		const uint32_t renderFeatures = RenderFeatures{}.asUint;
		commandList->SetGraphicsRoot32BitConstant(0, renderFeatures, 8);
	}

	void RenderEnd(ID3D12GraphicsCommandList10* commandList, TemporaryDescriptorHeap& descriptorHeap)
	{
		PIXEndEvent(commandList);
		ResourceTransitions(commandList,
			{
				albedoRT->Barrier(ResourceState::RenderTarget, ResourceState::ReadCS),
				normalsRT->Barrier(ResourceState::RenderTarget, ResourceState::ReadCS),
				depthBuffer->Barrier(ResourceState::DepthWrite, ResourceState::ReadCS),
				velocitiesRT.Barrier(ResourceState::RenderTarget, ResourceState::ReadCS)
			});

		commandList->SetPipelineState(copyDepthToDepthPyramidePso.Get());
		uint32_t rootConstants[] = {
			depthBuffer->srvId,
			depthPyramide.srvId
		};
		commandList->SetComputeRoot32BitConstants(0, _countof(rootConstants), rootConstants, 0);
		const int dispatchSizeX = DivisionRoundUp(depthPyramide.properties.width, 8);
		const int dispatchSizeY = DivisionRoundUp(depthPyramide.properties.height, 8);
		commandList->Dispatch(dispatchSizeX, dispatchSizeY, 1);

		ResourceTransitions(commandList, { BarrierCSWriteToRead(depthPyramide) });

		MipGenerator::GenerateMipsHardwareFiltering(commandList, descriptorHeap, depthPyramide, MipGenerator::SampleMode::MinimumMaximum, L"Depth Pyramide Generation");
		PIXEndEvent(commandList);
	}

	void Lighting(ID3D12GraphicsCommandList10* commandList,
		const RWTexture& output,
		DescriptorHeap::Id ssaoBufferSrvId,
		DescriptorHeap::Id indirectDiffuseBufferSrvId,
		DescriptorHeap::Id sssrBufferSrvId,
		BufferHeap::Offset lightingDataBufferOffset)
	{
		DispatchComputePass(commandList,
			deferredShadingPso.Get(),
			{
				output.uavId,
				ssaoBufferSrvId,
				indirectDiffuseBufferSrvId,
				sssrBufferSrvId,
				lightingDataBufferOffset,
			},
			{ .dispatchX = output.properties.width, .dispatchY = output.properties.height, .groupX = clusteredShadingTileSizeX, .groupY = clusteredShadingTileSizeY },
			L"GBuffer Lighting Pass");
	}

	void FrameEnd(ID3D12GraphicsCommandList10* commandList)
	{
		ResourceTransitions(commandList,
			{
				albedoRT.Other().Done(ResourceState::ReadCS, D3D12_BARRIER_LAYOUT_RENDER_TARGET),
				normalsRT.Other().Done(ResourceState::ReadCS, D3D12_BARRIER_LAYOUT_RENDER_TARGET),
				depthBuffer.Other().Done(ResourceState::ReadCS, D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE),
				depthPyramide.Done(ResourceState::ReadAny, D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_UNORDERED_ACCESS),
				velocitiesRT.Done(ResourceState::ReadCS, D3D12_BARRIER_LAYOUT_RENDER_TARGET)
			});

		albedoRT.Flip();
		normalsRT.Flip();
		depthBuffer.Flip();
	}

	SrvIds GetSrvIds() 
	{
		return 
		{
			.albedo = albedoRT.Current().srvId,
			.previousAlbedo = albedoRT.Other().srvId,
			.normals = normalsRT.Current().srvId,
			.previousNormals = normalsRT.Current().srvId,
			.velocities = velocitiesRT.srvId,
			.depth = depthBuffer.Current().srvId,
			.previousDepth = depthBuffer.Other().srvId,
			.depthPyramide = depthPyramide.srvId,
		};
	}
}

void UI::MaterialSettings::MenuEntry()
{
	if (ImGui::CollapsingHeader("Global Material Options", ImGuiTreeNodeFlags_None))
	{
		ImGui::Checkbox("Turn on Albedo Maps", &useAlbedoMaps);
		ImGui::Checkbox("Turn on Normal Maps", &useNormalMaps);
		ImGui::Checkbox("Turn on Roughness Maps", &useRoughnessMaps);
		ImGui::DragFloat("Roughness", &defaultRoughness, 0.005f, 0.0f, 1.0f, "%.2f");
		ImGui::Checkbox("Turn on Metallic Maps", &useMetallicMaps);
		ImGui::DragFloat("Metalness", &defaultMetalness, 0.005f, 0.0f, 1.0f, "%.2f");
	}
}
