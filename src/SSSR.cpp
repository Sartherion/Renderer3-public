#include "stdafx.h"
#include "SSSR.h"

#include "BufferMemory.h"
#include "Texture.h"

namespace SSSR
{
	static DXGI_FORMAT rayHitBufferFormat = DXGI_FORMAT_R16G16B16A16_UNORM;
	static DXGI_FORMAT specularReflectionBufferFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
	static const uint32_t haltonSamplesCount = 16;

	static RWTexture rayHitBuffer;
	static PingPong<RWTexture> specularReflectionBuffer;

	static ComPtr<ID3D12PipelineState> raymarchPso;
	static ComPtr<ID3D12PipelineState> sssrLighting;

	void Init(ID3D12Device10* device, uint32_t width, uint32_t height, DescriptorHeap& descriptorHeap, BufferHeap& bufferHeap)
	{
		rayHitBuffer = CreateRWTexture(
			device,
			{ .format = rayHitBufferFormat, .width = width, .height = height },
			descriptorHeap,
			L"RayHitBuffer");

		specularReflectionBuffer = CreatePingPongRWTextures(
			device,
			{ .format = specularReflectionBufferFormat, .width = width, .height = height },
			descriptorHeap,
			L"SpecularReflectionBuffer");

		raymarchPso = CreateComputePso(device, { .cs = LoadShaderBinary(L"content\\shaderbinaries\\SSSRRaymarchCS.cso").Get() });
		sssrLighting = CreateComputePso(device, { .cs = LoadShaderBinary(L"content\\shaderbinaries\\SSSRLightingCS.cso").Get() });

		bufferSrvId = specularReflectionBuffer->srvId;
	}

	void Render(ID3D12GraphicsCommandList10* commandList, DescriptorHeap::Id previousLitBufferSrvId, BufferHeap::Offset lightsDataBufferOffset)
	{
		PIXScopedEvent(commandList, PIX_COLOR_DEFAULT, "SSSR");
		DispatchComputePass(commandList,
			raymarchPso.Get(),
			{
				rayHitBuffer.uavId,
			},
			{ .dispatchX = rayHitBuffer.properties.width, .dispatchY = rayHitBuffer.properties.height },
			L"Ray Marching");

		ResourceTransitions(commandList, { BarrierCSWriteToRead(rayHitBuffer) });

		DispatchComputePass(commandList,
			sssrLighting.Get(),
			{
				specularReflectionBuffer->uavId,
				rayHitBuffer.srvId,
				specularReflectionBuffer.Other().srvId,
				previousLitBufferSrvId,
				lightsDataBufferOffset
			},
			{ .dispatchX = specularReflectionBuffer->properties.width, .dispatchY = specularReflectionBuffer->properties.height },
			L"Lighting");

		ResourceTransitions(commandList,
			{
				BarrierCSWriteToRead(*specularReflectionBuffer),
				rayHitBuffer.Done(ResourceState::ReadCS, D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_UNORDERED_ACCESS)
			});

		bufferSrvId = specularReflectionBuffer->srvId;
	}

	void FrameEnd(ID3D12GraphicsCommandList10* commandList)
	{
		ResourceTransitions(commandList,
			{
				specularReflectionBuffer.Current().Done(ResourceState::ReadCS, D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_SHADER_RESOURCE),
				specularReflectionBuffer.Other().Done(ResourceState::ReadCS, D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_UNORDERED_ACCESS),
			});

		specularReflectionBuffer.Flip();
	}
}

void UI::SSSRSettings::MenuEntry()
{
	if (ImGui::CollapsingHeader("SSSR Options", ImGuiTreeNodeFlags_None))
	{
		ImGui::DragInt("Maximum Steps Count", &maxHierarchicalSteps, 5.0f, 1, 500);
		ImGui::DragInt("Maximum Conetracing Mip Level", &maxConetracingMip, 5.0f, 0, 12);
		ImGui::DragFloat("Intensity", &intensity, 0.005f, 0.0f, FLT_MAX, "%.2f");
		ImGui::DragFloat("Bias", &bias, 0.005f, 0.0f, 1.0f, "%.02f");
		ImGui::DragFloat("Z Thickness", &depthBufferThickness, 0.005f, 0.0f, 10.0f);
		ImGui::DragFloat("Depth/Normal Similarity Threshold", &depthNormalSimilarityThreshold, 0.005f, 0.0f, 1.0f, "%.5f");
		ImGui::DragFloat("Border Fade Start", &borderFadeStart, 0.005f, 0.0f, 1.0f, "%.2f");
		ImGui::DragFloat("Border Fade End", &borderFadeEnd, 0.005f, 0.0f, 1.0f, "%.2f");
		ImGui::DragFloat("Parallax Threshold", &parallaxThreshold, 0.005f, 0.0f, 1.0f, "%.5f");
		ImGui::DragFloat("History Weight", &historyWeight, 0.005f, 0.0f, 1.0f, "%.2f");
		if (ImGui::DragFloat("Fallback Roughness Begin", &iblFallbackRougnessThresholdBegin, 0.005f, 0.0f, 1.0f, "%.2f"))
		{
			iblFallbackRougnessThresholdEnd = std::max(iblFallbackRougnessThresholdBegin, iblFallbackRougnessThresholdEnd);
			iblFallbackRougnessThresholdEnd += 0.0001f;
		}
		if (ImGui::DragFloat("Fallback Roughness End", &iblFallbackRougnessThresholdEnd, 0.005f, 0.0f, 1.0f, "%.2f"))
		{
			iblFallbackRougnessThresholdBegin = std::min(iblFallbackRougnessThresholdBegin, iblFallbackRougnessThresholdEnd);
			iblFallbackRougnessThresholdEnd += 0.0001f;
		}
	}
}