#pragma once
#include "Camera.h"
#include "FrameConstants.h"
#include "Light.h"
#include "PostProcess.h"
#include "TAA.h"

struct TemporaryDescriptorHeap;

namespace UI
{
	struct AppMenuBase;

	struct Context
	{
		SharedSettings sharedSettings;
		ShadowSettings directionalShadowSettings;
		ShadowSettings omnidirectionalShadowSettings = { .depthBias = 1200, .slopeScaledDepthBias = 1.2 };
		PostProcessSettings postProcessSettings;
		TAASettings taaSettings;
		AppMenuBase* appMenu = nullptr;

		MovementSpeed cameraMovementSpeed;
		bool isFocusDebugCameraWindow = false;
		ShadowMaps* cascadedShadowMapsPtr = nullptr;

		void Init(ShadowMaps& cascadedShadowMaps, AppMenuBase& appMenu);
		void Update(TemporaryDescriptorHeap& descriptorHeap);
	};

	inline bool mouseOverUI = false;

	void Init(HWND mainWindow,
		ID3D12Device10* device,
		DescriptorHeap& descriptorHeap,
		uint32_t width,
		uint32_t height,
		int framesInFLightCount);

	void FrameBegin();
	void Render(ID3D12GraphicsCommandList10* commandList, D3D12_CPU_DESCRIPTOR_HANDLE linearRtvHandle);
	void Shutdown();

	void TextureWindow(TemporaryDescriptorHeap& descriptorHeap, const TextureSubresource& texture, bool& open, LPCSTR title = "");
}

namespace UI
{
	void DrawGlobalInformationWindow(bool& open); 
}
