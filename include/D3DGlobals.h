#pragma once
#include "D3DUtility.h"
#include "RenderTarget.h"

namespace D3D
{
	constexpr DXGI_FORMAT backbufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	constexpr uint32_t swapChainBufferCount = 2;
	constexpr bool VSyncDisabled = false;

	constexpr DXGI_FORMAT HDRRenderTargetFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;

	constexpr float nearZ = .1f;
	constexpr float farZ = 50.0f;
	constexpr DXGI_FORMAT depthStencilFormat = DXGI_FORMAT_D32_FLOAT;
}

struct BufferHeap;
struct DescriptorHeap;
struct RtvHeap;
struct DsvHeap;
struct StackAllocator;
struct PersistentAllocator;

namespace D3D
{
	extern BufferHeap globalStaticBuffer;
	extern DescriptorHeap descriptorHeap;
	extern RtvHeap rtvHeap;
	extern DsvHeap dsvHeap;
	extern StackAllocator stackAllocator; 
	extern PersistentAllocator persistentAllocator;
}

namespace D3D
{
	inline PingPong<RenderTarget> mainRenderTarget;
	inline TemporaryRWTexturePool temporaryHdrBufferPool;
}

namespace D3D
{
	void InitGlobalState(ID3D12Device10* device, uint32_t renderTargetWidth, uint32_t renderTargetHeight);
	void PrepareCommandList(ID3D12GraphicsCommandList10* commandList, const DescriptorHeap& descriptorHeap, const BufferHeap& globalStaticBuffer);
	void Shutdown();
}
