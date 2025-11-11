#include "stdafx.h"
#include "IndirectDiffuse.h"

#include "Geometry.h"
#include "MipGeneration.h"
#include "Texture.h"

namespace IndirectDiffuse
{
	const uint32_t scaleFactor = 1;
	const DXGI_FORMAT format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	static uint32_t width;
	static uint32_t height;

	static ComPtr<ID3D12PipelineState> traceRaysPso;
	static ComPtr<ID3D12PipelineState> preBlurPso;
	static ComPtr<ID3D12PipelineState> reprojectionPso;
	static ComPtr<ID3D12PipelineState> historyFixPso;
	static ComPtr<ID3D12PipelineState> blurPso;

	static RWTexture temporaryBuffer;
	static PingPong<RWTexture> buffer;
	static PingPong<RWTexture> accumulatedFramesCountBuffer;

	static void InitPsos(ID3D12Device10* device);

	void Init(ID3D12Device10* device, uint32_t renderWidth, uint32_t renderHeight, DescriptorHeap& descriptorHeap)
	{
		InitPsos(device);

		width = renderWidth / scaleFactor;
		height = renderHeight / scaleFactor;

		buffer = CreatePingPongRWTextures(device,
			{ .format = format, .width = width, .height = height, .arraySize = 4 },
			descriptorHeap,
			L"Indirect Diffuse Buffer");

		temporaryBuffer = CreateRWTexture(device,
			{ .format = format, .width = width, .height = height, .arraySize = 4, .mipCount = 5 },
			descriptorHeap,
			L"Indirect Diffuse Temporary Buffer");

		accumulatedFramesCountBuffer = CreatePingPongRWTextures(device, 
			{ .format = DXGI_FORMAT_R16_FLOAT, .width = width, .height = height},
			descriptorHeap,
			L" Indirect Diffuse Accumulated Frames Count Buffer");

		bufferSrvId = buffer.Other().srvId;
	}

	void Render(ID3D12GraphicsCommandList10* commandList,
		TemporaryDescriptorHeap& descriptorHeap,
		const TlasData& tlasData,
		DescriptorHeap::Id skyboxsrvId,
		BufferHeap::Offset lightsDataOffset,
		BufferHeap::Offset aoBufferOffset,
		const UI::IndirectDiffuseSettings& settings)
	{
		if (settings.isActive)
		{
			PIXScopedEvent(commandList, PIX_COLOR_DEFAULT, "Indirect Diffuse");
			DispatchComputePass(commandList,
				traceRaysPso.Get(),
				{
					temporaryBuffer.uavId,
					tlasData.accelerationStructureSrvId,
					skyboxsrvId,
					lightsDataOffset,
					tlasData.instanceGeometryDataOffset
				},
				{ .dispatchX = width, .dispatchY = height },
				L"Trace per Pixel Rays");

			ResourceTransitions(commandList, {
				BarrierCSWriteToRead(temporaryBuffer),
				});

			DispatchComputePass(commandList,
				preBlurPso.Get(),
				{
					buffer.Current().uavId,
					temporaryBuffer.srvId,
				},
				{ .dispatchX = width, .dispatchY = height },
				L"Pre-Blur");

			ResourceTransitions(commandList, {
				BarrierCSWriteToRead(buffer.Current()),
				BarrierCSReadToWrite(temporaryBuffer),
				});

			DispatchComputePass(commandList,
				reprojectionPso.Get(),
				{
					temporaryBuffer.uavId,
					buffer.Current().srvId,
					buffer.Other().srvId,
					accumulatedFramesCountBuffer.Current().uavId,
					accumulatedFramesCountBuffer.Other().srvId,
				},
				{ .dispatchX = width, .dispatchY = height },
				L"Reprojection");

			ResourceTransitions(commandList, {
				BarrierCSReadToWrite(buffer.Current()),
				BarrierCSReadToWrite(buffer.Other()),
				BarrierCSWriteToRead(temporaryBuffer),
				BarrierCSWriteToRead(accumulatedFramesCountBuffer.Current()),
				accumulatedFramesCountBuffer.Other().Done(ResourceState::ReadCS, D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_UNORDERED_ACCESS)
				});

			MipGenerator::GenerateMipsHardwareFiltering(commandList,
				descriptorHeap,
				temporaryBuffer.GetSubresource({ .mipRange = {.count = 5}, .arrayRange = {.count = settings.denoiseInSHSpace ? 3u : 1u } }),
				MipGenerator::SampleMode::Bilinear, L"Mip Generation");

			// transition only mip 0 of the array elements to write state
			ResourceTransitions(commandList, {
				BarrierCSReadToWrite(temporaryBuffer.GetSubresource({.arrayRange = {.count = 3 } })),
				});

			DispatchComputePass(commandList,
				historyFixPso.Get(),
				{
					temporaryBuffer.uavId,
					temporaryBuffer.srvId,
					accumulatedFramesCountBuffer->srvId
				},
				{ .dispatchX = width, .dispatchY = height },
				L"History Fix");

			ResourceTransitions(commandList, {
				BarrierCSWriteToRead(temporaryBuffer.GetSubresource({.arrayRange = {.count = 3 } })),
				});

			DispatchComputePass(commandList,
				blurPso.Get(),
				{
					buffer.Other().uavId,
					temporaryBuffer.srvId,
					accumulatedFramesCountBuffer->srvId,
					aoBufferOffset
				},
				{ .dispatchX = width, .dispatchY = height },
				L"Blur");

			ResourceTransitions(commandList, {
				temporaryBuffer.Done(ResourceState::ReadCS, D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_UNORDERED_ACCESS),
				BarrierCSWriteToRead(buffer.Other()),
				});

			accumulatedFramesCountBuffer.Flip();
			bufferSrvId = buffer.Other().srvId;
		}
		else
		{
			bufferSrvId = DescriptorHeap::InvalidId;
		}
	}

	void InitPsos(ID3D12Device10* device)
	{
		traceRaysPso = CreateComputePso(device, { .cs = LoadShaderBinary(L"content\\shaderbinaries\\IndirectDiffuseTraceRaysCS.cso").Get() });
		preBlurPso = CreateComputePso(device, { .cs = LoadShaderBinary(L"content\\shaderbinaries\\IndirectDiffusePreBlurCS.cso").Get() });
		reprojectionPso = CreateComputePso(device, { .cs = LoadShaderBinary(L"content\\shaderbinaries\\IndirectDiffuseReprojectionCS.cso").Get() });
		historyFixPso = CreateComputePso(device, { .cs = LoadShaderBinary(L"content\\shaderbinaries\\IndirectDiffuseHistoryFixCS.cso").Get() });
		blurPso = CreateComputePso(device, { .cs = LoadShaderBinary(L"content\\shaderbinaries\\IndirectDiffuseBlurCS.cso").Get() });
	}

}

void UI::IndirectDiffuseSettings::MenuEntry()
{
	if (ImGui::CollapsingHeader("Indirect Diffuse Options", ImGuiTreeNodeFlags_None))
	{
		ImGui::Checkbox("Is Active##IndirectDiffuse", &isActive);
		ImGui::DragFloat("RTAO Hit Distance Threshold", &rtaoHitDistanceThreshold, 0.005f, 0.0f, 5.0f, "%.01f");
		ImGui::DragFloat("Recurrent Blur Radius", &blurRadius, 0.005f, 0.0f, 8.0f, "%.01f");
		ImGui::DragFloat("Bilateral Normal Weight Exponent", &normalWeightExponent, 0.005f, 1.0f, 10.0f, "%.01f");
		ImGui::DragFloat("Pre-Blur Radius", &preBlurRadius, 0.005f, 0.0f, 10.0f, "%.01f");
		ImGui::DragFloat("Disocclusion Depth Threshold", &disocclusionDepthThreshold, 0.005f, 0.0f, 1.0f, "%.01f");
		ImGui::DragFloat("History Fix Bilteral Depth Scale", &historyFixBilateralDepthScale, 0.005f, 0.0f, 10.0f, "%.001f");
		if (ImGui::Checkbox("Denoise in SH Space", &denoiseInSHSpace)) 
		{
			resetHistoryThisFrame = true;
		}
		else
		{
			resetHistoryThisFrame = false;
		}
	}
}
