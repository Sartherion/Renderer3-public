#include "stdafx.h"
#include "D3DGlobals.h"

#include "Buffer.h"
#include "D3DDrawHelpers.h"
#include "FrameConstants.h"
#include "Geometry.h"
#include "Texture.h"


namespace D3D
{
	BufferHeap globalStaticBuffer;
	DescriptorHeap descriptorHeap;
	RtvHeap rtvHeap;
	DsvHeap dsvHeap;
	StackAllocator stackAllocator; 
	PersistentAllocator persistentAllocator;

	uint32_t rtvHandleSize; 
	uint32_t dsvHandleSize;
	uint32_t srvHandleSize;

	ComPtr<ID3D12RootSignature> rootSignature;

	static const uint32_t dsvHeapSize = 1024;
	static const uint32_t rtvHeapSize = 2 * 4096;
	static const uint32_t descriptorHeapSize = 16 * 1024;
	static const uint32_t globalStaticBufferSize = 100 * 1024 * 1024;
	static const uint32_t stackAllocatorChunkSizeByte = 128 * 1024 * 1024; 
	static const uint32_t persistentAllocatorSize = 128 * 1024 * 1024;

	static const uint32_t temporaryHdrTexturesCount = 3;

	//debug buffer properties
	static const uint32_t debugBufferByteSize = 1024 * 4;
	static RWBufferResource debugBuffer; 
	
	//debug texture properties 
	static const uint32_t debugTextureArraySize = 13;
	static const DXGI_FORMAT debugTextureFormat = DXGI_FORMAT_R32G32B32A32_UINT;
	static RWTexture debugTexture; 

	void InitGlobalState(ID3D12Device10* device, uint32_t renderTargetWidth, uint32_t renderTargetHeight)
	{
		//Create universal root signature
		auto universalRs = LoadShaderBinary(L"content\\shaderbinaries\\UniversalRootSignature.cso");
		CheckForErrors(device->CreateRootSignature(0, universalRs->GetBufferPointer(), universalRs->GetBufferSize(), IID_PPV_ARGS(&rootSignature)));

		//Needs to be called before the helpers below are used 
		InitializeUtility(device);

		//descriptor heap globals
		D3D::rtvHandleSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		D3D::dsvHandleSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
		D3D::srvHandleSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		// Create descriptor heaps
		descriptorHeap.Init(device, descriptorHeapSize);
		rtvHeap.Init(device, rtvHeapSize);
		dsvHeap.Init(device, dsvHeapSize);

		// Reserve general purpose GPU and main memory
		globalStaticBuffer.Init(device, globalStaticBufferSize, L"Global Static Buffer");
		stackAllocator.Init(stackAllocatorChunkSizeByte, 10);
		persistentAllocator.Init(persistentAllocatorSize);

		//reserve DescriptorId 0 for debug buffer
		debugBuffer = CreateRWBufferResource(device,
			{ .size = debugBufferByteSize, .name = L"DebugBuffer" },
			D3D12_HEAP_TYPE_GPU_UPLOAD);
		{
			DescriptorHeap::Id debugBufferUavId = CreateUavOnHeap(descriptorHeap, debugBuffer);
			assert(debugBufferUavId == 0); //for simplicity the debug buffer can be used from any shader with DescriptorId 0
		}

		//reserve DescriptorId 1 for debug texture
		debugTexture = CreateRWTexture(device,
			{ .format = debugTextureFormat, .width = renderTargetWidth, .height = renderTargetHeight, .arraySize = debugTextureArraySize },
			descriptorHeap, L"DebugTexture");
		assert(debugTexture.uavId == 2);

		// Create main and temporary render targets
		const uint32_t renderTargetMipCount = ComputeMaximumMipLevel(renderTargetWidth, renderTargetHeight);
		mainRenderTarget = CreatePingPongRenderTarget(device,
			{ .format = D3D::HDRRenderTargetFormat, .width = renderTargetWidth, .height = renderTargetHeight, .mipCount = renderTargetMipCount },
		D3D::descriptorHeap,
		L"Main HDR Render Target",
		TextureUsage::ReadWrite);

		temporaryHdrBufferPool.Init(device,
			descriptorHeap,
			{ .format = HDRRenderTargetFormat, .width = renderTargetWidth, .height = renderTargetHeight, .mipCount = renderTargetMipCount },
			temporaryHdrTexturesCount,
			L"Temporary HDR RW Textures");

		InitDrawHelpers(device, globalStaticBuffer, HDRRenderTargetFormat, depthStencilFormat);
		Frame::Init(device, globalStaticBuffer, descriptorHeap);
	}

	void Shutdown()
	{
		temporaryHdrBufferPool.Free();
	}

	void PrepareCommandList(ID3D12GraphicsCommandList10* commandList, const DescriptorHeap& descriptorHeap, const BufferHeap& globalStaticBuffer)
	{
		ID3D12DescriptorHeap* descHeapsDummy[] = { descriptorHeap.heap.Get() };
		commandList->SetDescriptorHeaps(1, descHeapsDummy);

		commandList->SetGraphicsRootSignature(D3D::rootSignature.Get());
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		commandList->SetComputeRootSignature(D3D::rootSignature.Get());

		//Bind global buffer memory
		commandList->SetGraphicsRootShaderResourceView(1, globalStaticBuffer.parentBuffer.resource->GetGPUVirtualAddress());
		commandList->SetComputeRootShaderResourceView(1, globalStaticBuffer.parentBuffer.resource->GetGPUVirtualAddress());
	}
}
