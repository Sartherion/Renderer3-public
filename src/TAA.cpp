#include "stdafx.h"
#include "TAA.h"

#include "Texture.h"

namespace TAA
{
	static const DXGI_FORMAT staticVelociytBufferFormat = DXGI_FORMAT_R16G16_FLOAT;
	static RWTexture staticVelocityBuffer; 
	static PingPong<RWTexture> historyBuffer;
	
	static ComPtr<ID3D12PipelineState> resolvePso;
	static ComPtr<ID3D12PipelineState> reprojectionPso;

	void Init(ID3D12Device10* device, DXGI_FORMAT format, uint32_t width, uint32_t height, DescriptorHeap& descriptorHeap)
	{
		historyBuffer.Current() = CreateRWTexture(device,
			{ .format = format, .width = width, .height = height },
			descriptorHeap,
			L"HistoryBuffer0",
			TextureMemoryType::Default,
			D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_SHADER_RESOURCE);

		historyBuffer.Other() = CreateRWTexture(device,
			{ .format = format, .width = width, .height = height },
			descriptorHeap,
			L"HistoryBuffer1");

		resolvePso = CreateComputePso(device, { .cs = LoadShaderBinary(L"content\\shaderbinaries\\TAAResolveCS.cso").Get() });
	}

	Texture& Resolve(ID3D12GraphicsCommandList10* commandList, Texture& input, bool isActive)
	{
		if (!isActive)
		{
			return input;
		}

		PIXScopedEvent(commandList, PIX_COLOR_DEFAULT, "TAA Resolve");
		assert(input.properties.width == historyBuffer->properties.width
			&& input.properties.height == historyBuffer->properties.height);

		DispatchComputePass(commandList,
			resolvePso.Get(),
			{
				input.srvId,
				historyBuffer.Current().srvId, 
				historyBuffer.Other().uavId,
				staticVelocityBuffer.srvId,
			},
			{ .dispatchX = input.properties.width, .dispatchY = input.properties.width });

		ResourceTransitions(commandList,
			{
				historyBuffer.Current().Barrier(ResourceState::ReadAny, ResourceState::WriteCS),
				historyBuffer.Other().Barrier(ResourceState::WriteCS, ResourceState::ReadAny)
			});

		historyBuffer.Flip();

		return historyBuffer.Other();
	}
}

void UI::TAASettings::MenuEntry()
{
	if (ImGui::CollapsingHeader("TAA Settings", ImGuiTreeNodeFlags_None))
	{
		ImGui::Checkbox("Activate##TAA", &useTaa);
	}
}
