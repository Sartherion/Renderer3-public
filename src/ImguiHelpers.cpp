#include "stdafx.h"
#include "ImguiHelpers.h"

#include "App.h"
#include "D3DGlobals.h"
#include "DescriptorHeap.h"
#include "Texture.h"

namespace UI
{
	void Init(HWND mainWindow,
		ID3D12Device10* device,
		DescriptorHeap& descriptorHeap,
		uint32_t width,
		uint32_t height,
		int framesInFLightCount)
	{
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		io.DisplaySize = { static_cast<float>(width), static_cast<float>(height) };
		io.FontGlobalScale = 1.5f;
		ImGui::GetStyle().ScaleAllSizes(1.5f);
		ImGui_ImplWin32_Init(mainWindow);
		{
			DescriptorHeap::Id imGuiStartId = descriptorHeap.Allocate(framesInFLightCount);
			ImGui_ImplDX12_Init(device,
				framesInFLightCount,
				DirectX::MakeLinear(D3D::backbufferFormat),
				descriptorHeap.heap.Get(),
				descriptorHeap.GetDescriptorHandleAt(imGuiStartId),
				descriptorHeap.GetDescriptorHandleAt(imGuiStartId));
		}
	}

	void FrameBegin()
	{
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
	}

	void Render(ID3D12GraphicsCommandList10* commandList, D3D12_CPU_DESCRIPTOR_HANDLE linearRtvHandle)
	{
		commandList->OMSetRenderTargets(1, &linearRtvHandle, true, nullptr);

		ImGui::Render();
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
	}

	void Shutdown()
	{
		ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
	}

	void TextureWindow(TemporaryDescriptorHeap& descriptorHeap, const TextureSubresource& texture, bool& open, LPCSTR title)
	{
		if (!open)
		{
			return;
		}
		ImGui::Begin(title, &open);
		static int elementsPerLineCount = 5;
		static float elementScale = 0.25f; //@todo: all windows will share the element size, but ok for now
		if (ImGui::CollapsingHeader("Window Settings", ImGuiTreeNodeFlags_None))
		{
			ImGui::DragFloat("Element Scale", &elementScale, 0.005f, 0.0f, 1.0f, "%.2f");
			ImGui::DragInt("Steps Count", &elementsPerLineCount, 1.0f, 1, 16);
		}

		uint32_t elementsCount = 0;
		for (uint32_t mip = texture.range.mipRange.begin; mip < texture.range.mipRange.End(); mip++)
		{
			for (uint32_t arrayIndex = texture.range.arrayRange.begin; arrayIndex < texture.range.arrayRange.End(); arrayIndex++)
			{
				TextureSubresource singleSubresource = texture;
				singleSubresource.range.mipRange.begin= mip;
				singleSubresource.range.mipRange.count= 1;
				singleSubresource.range.arrayRange.begin = arrayIndex;
				singleSubresource.range.arrayRange.count = 1;
				D3D12_GPU_DESCRIPTOR_HANDLE handle = descriptorHeap.GetDescriptorHandleAt(CreateSrvOnHeap(descriptorHeap, singleSubresource));

				const uint32_t width = singleSubresource.parentResourcePtr->properties.width;
				const uint32_t height = singleSubresource.parentResourcePtr->properties.height;

				ImGui::Image((ImTextureID)handle.ptr, ImVec2(width * elementScale, height * elementScale));
				elementsCount++;
				if (elementsCount % elementsPerLineCount != 0)
				{
					ImGui::SameLine();
				}
			}
		}
		ImGui::End();
	}

	void Context::Init(ShadowMaps& cascadedShadowMaps, AppMenuBase& appMenu)
	{
		this->cascadedShadowMapsPtr = &cascadedShadowMaps;
		this->appMenu = &appMenu;
	}

	void Context::Update(TemporaryDescriptorHeap& descriptorHeap)
	{
		{
			if (appMenu)
			{
				appMenu->MenuEntry();
			}

			if (ImGui::CollapsingHeader("ShadowSettings", ImGuiTreeNodeFlags_None))
			{
				directionalShadowSettings.MenuEntry("Directional Shadows");
				omnidirectionalShadowSettings.MenuEntry("Point Light Shadows");
			}

			postProcessSettings.MenuEntry();
			sharedSettings.lightingSettings.MenuEntry();
			sharedSettings.materialSettings.MenuEntry();
			sharedSettings.sssrSettings.MenuEntry();
			sharedSettings.ssaoSettings.MenuEntry();
			sharedSettings.ddgiSettings.MenuEntry();
			sharedSettings.indirectDiffuseSettings.MenuEntry();
			taaSettings.MenuEntry();
			sharedSettings.pathtracerSettings.MenuEntry();
			sharedSettings.debugVisualizationSettings.MenuEntry();

			static bool showGlobalInformationWindow = true;
			if (ImGui::CollapsingHeader("Miscellaneous", ImGuiTreeNodeFlags_None))
			{
				auto& options = cameraMovementSpeed;
				ImGui::DragFloat("Camera Movement Speed", &options.moveSpeed, 1e-5f, 0.0f, 1.0f, "%.5f");

				ImGui::Checkbox("Show Resource Info", &showGlobalInformationWindow);
			}
			DrawGlobalInformationWindow(showGlobalInformationWindow);
		}

		TextureWindow(descriptorHeap, cascadedShadowMapsPtr->depthBuffer.GetSubresource(), sharedSettings.debugVisualizationSettings.isActiveCSMVisualization, "Cascaded Shadow Maps");

		ImGuiIO& io = ImGui::GetIO();
		mouseOverUI = io.WantCaptureMouse;
	}
}

void UI::DrawGlobalInformationWindow(bool& open)
{
	if (!open)
	{
		return;
	}

	const float averageFPS = 1000.0f / Frame::timingData.averageDeltaTimeMs;
	const float deltaTimeMs = Frame::timingData.deltaTimeMs;
	const uint32_t globalBufferSize = D3D::globalStaticBuffer.parentBuffer.size;
	const uint32_t globalBufferUsed = D3D::globalStaticBuffer.parentBuffer.size - D3D::globalStaticBuffer.allocator.storageReport().totalFreeSpace;
	const uint32_t descriptorHeapSize = D3D::descriptorHeap.elementMaxCount;
	const uint32_t descriptorHeapUsed = D3D::descriptorHeap.elementMaxCount - D3D::descriptorHeap.allocator.storageReport().totalFreeSpace;
	const uint32_t persistentAllocatorSize = D3D::persistentAllocator.totalSize;
	const uint32_t persistentAllocatorUsed = D3D::persistentAllocator.totalSize - D3D::persistentAllocator.allocator.storageReport().totalFreeSpace;
	const uint32_t stackAllocatorMaxUsage = D3D::stackAllocator.GetMaxUsageBytes();
	const uint32_t stackAllocatorReservedMemory = D3D::stackAllocator.GetReservedMemoryBytes();
	const uint32_t linearAllocatorMaxUsage = Frame::current->cpuMemory.diagnosis.maxSizeInUseByte;
	const uint32_t linearAllocatorReservedMemory = Frame::current->cpuMemory.allocator.GetReservedMemoryBytes();

	ImGui::Begin("Information: ", &open);
	char textBuffer[64];

	ImGui::Text("Frame Time");
	sprintf_s(textBuffer, "%f fps\n %f ms \n", averageFPS, deltaTimeMs);
	ImGui::PlotLines("", Frame::historyData.frameTime, Frame::historyData.frameCount, Frame::historyData.frameIndex, textBuffer, 0.0f, 50.0f, ImVec2(0, 100.0f));

	sprintf_s(textBuffer, " Render Target Size %u x %u", App::renderSettings.renderTargetWidth, App::renderSettings.renderTargetHeight);
	ImGui::Text(textBuffer);

	{
		float progress = static_cast<float>(globalBufferUsed) / globalBufferSize;
		sprintf_s(textBuffer, "Global Buffer: %u / %u bytes (%.1f %%)", globalBufferUsed, globalBufferSize, 100 * progress);
		ImGui::ProgressBar(progress, ImVec2(-1.0, 0), textBuffer);
	}

	{
		float progress = static_cast<float>(descriptorHeapUsed) / descriptorHeapSize;
		sprintf_s(textBuffer, "Descriptor Heap: %u / %u (%.1f %%)", descriptorHeapUsed, descriptorHeapSize, 100 * progress);
		ImGui::ProgressBar(progress, ImVec2(-1.0, 0), textBuffer);
	}

	{
		float progress = static_cast<float>(persistentAllocatorUsed) / persistentAllocatorSize;
		sprintf_s(textBuffer, "Persistent Allocator Usage / Reserved: %u / %u (%.1f %%)", persistentAllocatorUsed, persistentAllocatorSize, 100 * progress);
		ImGui::ProgressBar(progress, ImVec2(-1.0, 0), textBuffer);
	}

	{
		float progress = static_cast<float>(linearAllocatorMaxUsage) / linearAllocatorReservedMemory;
		sprintf_s(textBuffer, "Linear Allocator Usage / Reserved: %u / %u (%.1f %%)", linearAllocatorMaxUsage, linearAllocatorReservedMemory, 100 * progress);
		ImGui::ProgressBar(progress, ImVec2(-1.0, 0), textBuffer);
	}

	{
		float progress = static_cast<float>(stackAllocatorMaxUsage) / stackAllocatorReservedMemory;
		sprintf_s(textBuffer, "Stack Allocator Usage / Reserved: %u / %u (%.1f %%)", stackAllocatorMaxUsage, stackAllocatorReservedMemory, 100 * progress);
		ImGui::ProgressBar(progress, ImVec2(-1.0, 0), textBuffer);
	}
	ImGui::End();
}
