#include "stdafx.h"
#include "DebugDrawing.h"

#include "D3DDrawHelpers.h"
#include "DepthBuffer.h"
#include "DescriptorHeap.h"
#include "RenderTarget.h"

namespace DebugView
{
	RenderTarget renderTarget;
	static DepthBuffer depthBuffer;
	static PersistentBuffer<Texture2DDimensions> dimensionsBuffer;

	void Init(ID3D12Device10* device,
		DescriptorHeap& descriptorHeap,
		BufferHeap& bufferHeap,
		DXGI_FORMAT renderTargetFormat,
		DXGI_FORMAT depthBufferFormat,
		uint32_t width,
		uint32_t height)
	{
		renderTarget = CreateRenderTarget(
			device,
			{ .format = renderTargetFormat, .width = width, .height = height },
			descriptorHeap,
			L"Debug View Render Target",
			TextureUsage::Default,
			TextureMemoryType::Default,
			D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_SHADER_RESOURCE);

		depthBuffer = CreateDepthBuffer(device,
			{ .format = depthBufferFormat, .width = width, .height = height },
			descriptorHeap,
			L"Debug View Depth Buffer");

		dimensionsBuffer = CreatePersistentBuffer<Texture2DDimensions>(bufferHeap);
		dimensionsBuffer.Write(GetTexture2DDimensions(renderTarget));
	}

	void RenderBegin(ID3D12GraphicsCommandList10* commandList, BufferHeap::Offset cameraDataOffset, BufferHeap::Offset lightsDataBufferOffset)
	{
		PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, "Debug View Rendering");

		ResourceTransitions(commandList, { renderTarget.Barrier(ResourceState::ReadAny, ResourceState::RenderTarget) });
		//Clear Render targets / Depth Buffer and bind them
		Bind(commandList, { { renderTarget } }, depthBuffer);
		renderTarget.Clear(commandList);
		depthBuffer.Clear(commandList);

		BindFixedRenderGraphicsRootConstants(commandList,
			lightsDataBufferOffset,
			cameraDataOffset,
			dimensionsBuffer.offset, { .asBitfield = {.sampleIndirectSpecularMap = true, .sampleAoMap = true, .isDebugCamera = true} });
	}

	void RenderEnd(ID3D12GraphicsCommandList10* commandList)
	{
		ResourceTransitions(commandList, { renderTarget.Barrier(ResourceState::RenderTarget, ResourceState::ReadAny) });
		PIXEndEvent(commandList);
	}
}

void UI::DebugVisualizationSettings::MenuEntry()
{
	if (ImGui::CollapsingHeader("Debug Visualization", ImGuiTreeNodeFlags_None))
	{
		ImGui::Checkbox("Activate DDGI Probe Visualization", &isActiveDDGIVisualization);
		ImGui::Checkbox("Show Radiance instead of Irradiance", &isRadianceDDGIVisualization); 
		ImGui::Checkbox("Use Debug Camera", &isActiveDebugCamera);
		ImGui::Checkbox("Limit Debug Camera to Main Camera Viewport", &limitDebugCameraToMainCameraViewPort);
		ImGui::Checkbox("Show CSM Visualization", &isActiveCSMVisualization);
		ImGui::DragInt("Debug View Cascade Index", &debugCascadeIndex, 0.01f, 0, 10);
		ImGui::DragInt("Debug Cluster Per Cell Max Count", &debugClusteredPerCellMaxCount, 0.01f, 1, 100);

		static_assert(sizeof(DebugVisualizationSettings::NameLookup) / sizeof(const char*) == DebugVisualizationSettings::Count);
		if (ImGui::BeginCombo("RenderMode", DebugVisualizationSettings::NameLookup[renderMode]))
		{
			for (int i = 0; i < DebugVisualizationSettings::Count; i++)
			{
				if (ImGui::Selectable(DebugVisualizationSettings::NameLookup[i], renderMode == i))
				{
					renderMode = (DebugVisualizationSettings::RenderMode)i;
				}
			}
			ImGui::EndCombo();
		}
	}
}

bool UI::DebugCameraWindow(TemporaryDescriptorHeap& descriptorHeap, const TextureResource& texture, bool& open)
{
	if (!open)
	{
		return false;
	}

	bool isFocus = false;
	static bool lockWindow = true;
	ImGui::Begin("Debug Camera", &open, lockWindow ? ImGuiWindowFlags_NoMove : 0);

	ImGui::Checkbox("Lock Window", &lockWindow);

	D3D12_GPU_DESCRIPTOR_HANDLE handle = descriptorHeap.GetDescriptorHandleAt(CreateSrvOnHeap(descriptorHeap, texture));
	ImGui::Image((ImTextureID)handle.ptr, ImGui::GetContentRegionAvail());

	isFocus = ImGui::IsWindowFocused();
	ImGui::End();

	return isFocus;
}

