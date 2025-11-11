#include "stdafx.h"
#include "SSAO.h"

#include "Texture.h"


namespace SSAO
{
	static ComPtr<ID3D12PipelineState> mainPso, blurHorizontalPso, blurVerticalPso;
	static RWTexture buffer;
	static RWTexture intermediateBuffer;

	static void InitPsos(ID3D12Device10* device);

	void Init(ID3D12Device10* device, DescriptorHeap& descriptorHeap, uint32_t width, uint32_t height)
	{
		InitPsos(device);

		buffer = CreateRWTexture(device,
			{ .format = DXGI_FORMAT_R16G16B16A16_FLOAT, .width = width, .height = height },
			descriptorHeap,
			L"SSGI Buffer");
		intermediateBuffer = CreateRWTexture(device,
			{ .format = DXGI_FORMAT_R16G16B16A16_FLOAT, .width = width, .height = height },
			descriptorHeap,
			L"SSGI Intermediate");

		bufferSrvId = buffer.srvId;
	}

	void Render(ID3D12GraphicsCommandList10* commandList, BufferHeap::Offset ddgiDataOffset, DescriptorHeap::Id litBufferSrvId)
	{
		PIXScopedEvent(commandList, PIX_COLOR_DEFAULT, "SSGI");
		DispatchComputePass(commandList,
			mainPso.Get(),
			{
				buffer.uavId,
				litBufferSrvId,
				ddgiDataOffset
			},
			{ .dispatchX = buffer.properties.width, .dispatchY = buffer.properties.height });

		ResourceTransitions(commandList, { BarrierCSWriteToRead(buffer) });

		DispatchComputePass(commandList,
			blurHorizontalPso.Get(),
			{
				intermediateBuffer.uavId,
				buffer.srvId
			},
			{ .dispatchX = buffer.properties.width, .dispatchY = buffer.properties.height, .groupX = 32, .groupY = 1 },
			L"Horizontal Blur");

		ResourceTransitions(commandList, { 
			BarrierCSReadToWrite(buffer),
			BarrierCSWriteToRead(intermediateBuffer)
			});

		DispatchComputePass(commandList,
			blurVerticalPso.Get(),
			{
				buffer.uavId,
				intermediateBuffer.srvId
			},
			{ .dispatchX = buffer.properties.width, .dispatchY = buffer.properties.height, .groupX = 1, .groupY = 32 },
			L"Vertical Blur");

		ResourceTransitions(commandList, { 
			BarrierCSWriteToRead(buffer),
			intermediateBuffer.Done(ResourceState::ReadCS, D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_UNORDERED_ACCESS)
			});
	}

	D3D12_TEXTURE_BARRIER Done()
	{
		return buffer.Done(ResourceState::ReadCS, D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_UNORDERED_ACCESS);
	}

	void InitPsos(ID3D12Device10* device)
	{
		mainPso = CreateComputePso(device, { .cs = LoadShaderBinary(L"content\\shaderbinaries\\SSAOCS.cso").Get() });
		blurHorizontalPso = CreateComputePso(device, { .cs = LoadShaderBinary(L"content\\shaderbinaries\\SSAOBlurHorizontalCS.cso").Get() });
		blurVerticalPso = CreateComputePso(device, { .cs = LoadShaderBinary(L"content\\shaderbinaries\\SSAOBlurVerticalCS.cso").Get() });
	}
}

void UI::SSAOSettings::MenuEntry()
{

	if (ImGui::CollapsingHeader("SSAO Options", ImGuiTreeNodeFlags_None))
	{
		ImGui::Checkbox("Activate AO", &isActiveAO);
		ImGui::DragFloat("Radius", &radius, 0.005f, 0.0f, 5.0f, "%.05f");
		ImGui::DragFloat("Depth Buffer Thickness", &thickness, 0.005f, 0.0f, 5.0f, "%.05f");
		ImGui::DragInt("Steps Count", &stepsCount, 0.005f, 1, 128);
		ImGui::DragFloat("Blur Depth Sigma", &blurDepthSigma, 0.005f, 0.0f, 5.0f, "%.3f");
		ImGui::DragFloat("Blur Normal Sigma", &blurNormalSigma, 0.005f, 0.0f, 5.0f, "%.3f");
		ImGui::DragInt("Azimuthal Steps Count", &azimuthalDirectionsCount, 0.005f, 1, 128);
		ImGui::Checkbox("Jitter Offset", &useOffsetJitter);
	}
}
