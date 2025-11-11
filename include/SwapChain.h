#pragma once

struct RenderTarget;

struct SwapChain
{
	void Init(ID3D12Device10* device, 
		IDXGIFactory4* dxgiFactory, 
		ID3D12CommandQueue* commandQueue, 
		HWND mainWindow,
		uint32_t bufferCount, 
		DXGI_FORMAT displayFormat);

	void FrameBegin(ID3D12GraphicsCommandList10* commandList);
	[[nodiscard]]
	D3D12_TEXTURE_BARRIER Done() const; //needs to be called on a command List when rendering to back buffer is finished before calling present
	void Present(bool bAllowTearing = false);
	void Free();

	//@note: next to rtv for render target, during swapchain creation we store an additonal linear rtv, because this is what imgui requires
	D3D12_CPU_DESCRIPTOR_HANDLE GetLinearRtvHandle() const;

	uint32_t index = 0;
	uint32_t bufferCount;

	const RenderTarget* currentBackBuffer;
	ComPtr<IDXGISwapChain4> swapChain;
	RenderTarget* renderTargets;
};
