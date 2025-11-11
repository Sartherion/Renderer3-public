#pragma once
#include "Renderer.h"

struct LinearAllocator;
struct BufferHeap;
struct RWBufferResource;

namespace Frame
{
	struct TimingData;
}

namespace UI
{
	extern bool mouseOverUI;
}

namespace App
{
	inline LPCWSTR name = L"Renderer Test App";
	inline UI::AppMenuBase* menu;
	constexpr RenderSettings renderSettings =
	{
		.directionalLightsMaxCount = directionalLightsMaxCount,
		.shadowedPointLightsMaxCount = 25,
		.cubeMapsMaxCount = 1,
		.cubeMapFacesPerFrameUpdateCount = 1,
		.ddgiProbeSpacing = { 3.0f, 3.0f, 3.0f },
		.ddgiRelativeOffset = {-0.5f, -0.05f, -0.52f }
	};

	void Init(ID3D12Device10* device,
		ID3D12GraphicsCommandList10* commandList,
		PersistentAllocator& allocator,
		DescriptorHeap& descriptorHeap,
		BufferHeap& bufferHeap,
		RWBufferResource& scratchBuffer);

	RenderData Update(LinearAllocator& frameAllocator, const Frame::TimingData& timingData);
}

