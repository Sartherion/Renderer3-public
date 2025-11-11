#include "stdafx.h"
#include "PathTracer.h"

#include "D3DBarrierHelpers.h"
#include "Geometry.h"
#include "Texture.h"


namespace PathTracer
{
	static void Denoise(ID3D12GraphicsCommandList10* commandList, RWTexture& temporaryBuffer);

	static ComPtr<ID3D12PipelineState> primaryRayGenPso;
	static ComPtr<ID3D12PipelineState> bilateralDenoiseHorizontalPso;
	static ComPtr<ID3D12PipelineState> bilateralDenoiseVerticalPso;

	static RWTexture accumulationBuffer;
	static RWTexture resultBuffer;
	
	static bool isActive = false;

	void Init(ID3D12Device10* device, DescriptorHeap& descriptorHeap, uint32_t renderWidth, uint32_t renderHeight)
	{
		primaryRayGenPso = CreateComputePso(device, { .cs = LoadShaderBinary(L"content\\shaderbinaries\\PathtracingCS.cso").Get() });
		bilateralDenoiseHorizontalPso = CreateComputePso(device, { .cs = LoadShaderBinary(L"content\\shaderbinaries\\BilateralDenoiseHorizontalCS.cso").Get() });
		bilateralDenoiseVerticalPso = CreateComputePso(device, { .cs = LoadShaderBinary(L"content\\shaderbinaries\\BilateralDenoiseVerticalCs.cso").Get() });

		accumulationBuffer = CreateRWTexture(
			device,
			{ .format = DXGI_FORMAT_R32G32B32A32_FLOAT, .width = renderWidth, .height = renderHeight },
			descriptorHeap,
			L"PathTracingAccumulationBuffer");

		resultBuffer = CreateRWTexture(
			device,
			{ .format = DXGI_FORMAT_R16G16B16A16_FLOAT, .width = renderWidth, .height = renderHeight },
			descriptorHeap,
			L"PathTracingResultBuffer");
	}

	Texture* Render(ID3D12GraphicsCommandList10* commandList,
		RWTexture& temporaryBuffer, 
		const TlasData& tlasData, 
		DescriptorHeap::Id skyboxSrvId, 
		BufferHeap::Offset lightsDataOffset, 
		const UI::ReferencePathTracerSettings& settings)
	{
		isActive = false;
		if (settings.isActive)
		{
			isActive = true;
			PIXScopedEvent(commandList, PIX_COLOR_DEFAULT, "PathTracing");
			DispatchComputePass(commandList,
				primaryRayGenPso.Get(),
				{ .dispatchX = accumulationBuffer.properties.width, .dispatchY = accumulationBuffer.properties.height },
					accumulationBuffer.uavId.Id(),
					accumulationBuffer.properties.width,
					accumulationBuffer.properties.height,
					1.0f / accumulationBuffer.properties.width,
					1.0f / accumulationBuffer.properties.height,
					tlasData.accelerationStructureSrvId,
					tlasData.instanceGeometryDataOffset,
					skyboxSrvId,
					lightsDataOffset);

			ResourceTransitions(commandList, { BarrierCSWriteToRead(accumulationBuffer) });

			PathTracer::Denoise(commandList, temporaryBuffer);
			
			return &resultBuffer;
		}

		return nullptr;
	}

	void Denoise(ID3D12GraphicsCommandList10* commandList, RWTexture& temporaryBuffer)
	{
		assert(temporaryBuffer.HasSameDimensionsAs(accumulationBuffer));

		PIXScopedEvent(commandList, PIX_COLOR_DEFAULT, "PathTracingDenoising");

		// horizontal pass
		DispatchComputePass(commandList,
			bilateralDenoiseHorizontalPso.Get(),
			{ .dispatchX = accumulationBuffer.properties.width, .dispatchY = accumulationBuffer.properties.height, .groupX = 32, .groupY = 1 },
				accumulationBuffer.srvId.Id(),
				temporaryBuffer.uavId.Id());

		ResourceTransitions(commandList,
			{
				BarrierCSWriteToRead(temporaryBuffer),
				BarrierCSReadToWrite(accumulationBuffer)
			});

		// vertical pass
		DispatchComputePass(commandList,
			bilateralDenoiseVerticalPso.Get(),
			{ .dispatchX = accumulationBuffer.properties.width, .dispatchY = accumulationBuffer.properties.height, .groupX = 1, .groupY = 32 },
				temporaryBuffer.srvId.Id(),
				resultBuffer.uavId.Id());

		ResourceTransitions(commandList,
			{
				BarrierCSWriteToRead(resultBuffer),
				BarrierCSReadToWrite(temporaryBuffer)
			});
	}

	D3D12_TEXTURE_BARRIER Done()
	{
		if (isActive)
		{
			return resultBuffer.Done(ResourceState::ReadAny, D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_UNORDERED_ACCESS);
		}
		else
		{
			return {};
		}
	}
}

void UI::ReferencePathTracerSettings::MenuEntry()
{
	if (ImGui::CollapsingHeader("Reference Path Tracer Options", ImGuiTreeNodeFlags_None))
	{
		ImGui::Checkbox("Activate##Pathtracer", &isActive);
		ImGui::Checkbox("Denoise", &denoise);
		ImGui::Checkbox("Bosst Roughness", &boostRoughness);
		ImGui::Checkbox("Cast Shadows for Unshadowed Lights", &castShadowsForUnshadowedLights);
		ImGui::DragInt("Maximum Number of Bounces", &bounceMaximumCount, 5.0f, 1, 50);
		ImGui::DragFloat("Denoise Normal Sigma", &denoiseNormalSigma, 0.005f, 0.0f, 5.0f, "%.3f");
		ImGui::DragFloat("Denoise Depth Sigma", &denoiseDepthSigma, 0.005f, 0.0f, 5.0f, "%.3f");
	}
}
