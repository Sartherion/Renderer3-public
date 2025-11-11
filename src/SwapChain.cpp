#include "stdafx.h"
#include "SwapChain.h"

#include "RenderTarget.h"

void SwapChain::Init(ID3D12Device10* device, 
	IDXGIFactory4* dxgiFactory, 
	ID3D12CommandQueue* commandQueue, 
	HWND mainWindow,
	uint32_t bufferCount,
	DXGI_FORMAT displayFormat)
{
	renderTargets = new RenderTarget[bufferCount];
	this->bufferCount = bufferCount;
	index = 0;
	currentBackBuffer = &renderTargets[0];

	//Create swap chain
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc;
	{
		swapChainDesc.Width = 0;
		swapChainDesc.Height = 0;
		swapChainDesc.Stereo = false;
		swapChainDesc.Format = DirectX::MakeLinear(displayFormat);
		swapChainDesc.SampleDesc = { 1, 0 }; 
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.BufferCount = bufferCount;
		swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
		swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH | DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING | DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
		ComPtr<IDXGISwapChain1> tempSwapChain;
		CheckForErrors(dxgiFactory->CreateSwapChainForHwnd(commandQueue, mainWindow, &swapChainDesc, nullptr, nullptr, &tempSwapChain));
		tempSwapChain.As(&swapChain);
		swapChain->SetMaximumFrameLatency(40);

		swapChain->GetDesc1(&swapChainDesc);
	}

	//Create rtv for all swap chain buffers
	{
		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc;
		{
			// Flip swap chains don't allow the render target itself to have srgb formats,
			// only the view, cf. https://www.gamedev.net/forums/topic/670546-d3d12srgb-buffer-format-for-swap-chain/1000
			rtvDesc.Format = displayFormat;
			rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
			rtvDesc.Texture2D.MipSlice = 0;
			rtvDesc.Texture2D.PlaneSlice = 0;
		}

		D3D12_RENDER_TARGET_VIEW_DESC rtvDescLinear = rtvDesc;
		rtvDescLinear.Format = DirectX::MakeLinear(displayFormat);

		for (unsigned int i = 0; i < bufferCount; i++)
		{
			//Fill in RenderTarget struct manually
			TextureResource& textureResource = renderTargets[i];
			textureResource.properties.width = swapChainDesc.Width;
			textureResource.properties.height = swapChainDesc.Height;
			textureResource.properties.format = displayFormat;
			textureResource.properties.arraySize = 1;
			textureResource.properties.mipCount = 1;
			textureResource.properties.sampleCount = 1;
			textureResource.type = TextureType::RenderTarget;
			textureResource.usage = TextureUsage::DisallowShader;
			CheckForErrors(swapChain->GetBuffer(i, IID_PPV_ARGS(&textureResource.ptr)));
			wchar_t nameBuffer[32];
			swprintf_s(nameBuffer, L"Backbuffer%d", i);
			textureResource.ptr->SetName(nameBuffer);

			renderTargets[i].rtv = D3D::rtvHeap.Allocate(2);
			device->CreateRenderTargetView(textureResource.ptr.Get(), &rtvDesc, renderTargets[i].rtv.Handle());
			//Reserve an additonal rtv with linear format next to corresponding srgb rtv
			device->CreateRenderTargetView(textureResource.ptr.Get(), &rtvDescLinear, renderTargets[i].rtv.Handle().Offset(1));
			renderTargets[i].srvId = {};
		}
	}
}

void SwapChain::FrameBegin(ID3D12GraphicsCommandList10* commandList)
{
	ResourceTransitions(commandList,
		{
			currentBackBuffer->Barrier(ResourceState::Common, ResourceState::RenderTarget)
		});
}

void SwapChain::Present(bool bAllowTearing)
{
	swapChain->Present(0, bAllowTearing ? DXGI_PRESENT_ALLOW_TEARING : 0);
	index = (index + 1) % bufferCount;
	currentBackBuffer = &renderTargets[index];
}

D3D12_CPU_DESCRIPTOR_HANDLE SwapChain::GetLinearRtvHandle() const
{
	return currentBackBuffer->GetRtv().Offset(1);
}

void SwapChain::Free()
{
	if (renderTargets)
	{
		delete[] renderTargets;
	}
	index = 0;
	swapChain = nullptr;
}

D3D12_TEXTURE_BARRIER SwapChain::Done() const
{
	return currentBackBuffer->Done(ResourceState::RenderTarget, D3D12_BARRIER_LAYOUT_PRESENT);
}