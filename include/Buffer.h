#pragma once
#include "D3DBarrierHelpers.h"
#include "DescriptorHeap.h"

struct BufferResource
{
	ComPtr<ID3D12Resource1> resource;
	void* cpuPtr = nullptr;
	uint32_t size = 0;

	[[nodiscard]]
	D3D12_BUFFER_BARRIER Barrier(ResourceState from, ResourceState to) const;
};

struct RWBufferResource : BufferResource
{
};

struct BufferDesc;
BufferResource CreateBufferResource(ID3D12Device10* device,
	BufferDesc&& bufferDesc, 
	D3D12_HEAP_TYPE heapType = D3D12_HEAP_TYPE_GPU_UPLOAD, D3D12_RESOURCE_FLAGS additionalFlags = D3D12_RESOURCE_FLAG_NONE);

RWBufferResource CreateRWBufferResource(ID3D12Device10* device,
	BufferDesc&& bufferDesc,
	D3D12_HEAP_TYPE heapType = D3D12_HEAP_TYPE_GPU_UPLOAD);

BufferResource CreateConstantBufferResource(ID3D12Device10* device,
	BufferDesc&& bufferDesc,
	D3D12_HEAP_TYPE heapType = D3D12_HEAP_TYPE_GPU_UPLOAD);

D3D12_SHADER_RESOURCE_VIEW_DESC GetRawSrvDesc(const BufferResource& buffer, uint32_t firstElement = 0);
D3D12_SHADER_RESOURCE_VIEW_DESC GetTypedSrvDesc(const BufferResource& buffer, DXGI_FORMAT format, uint32_t firstElement = 0);
D3D12_SHADER_RESOURCE_VIEW_DESC GetStructuredSrvDesc(const BufferResource& buffer, uint32_t stride, uint32_t firstElement = 0);

D3D12_UNORDERED_ACCESS_VIEW_DESC GetRawUavDesc(const RWBufferResource& rwBuffer, uint32_t firstElement = 0);
D3D12_UNORDERED_ACCESS_VIEW_DESC GetTypedUavDesc(const RWBufferResource& rwBuffer, DXGI_FORMAT format, uint32_t firstElement = 0);
D3D12_UNORDERED_ACCESS_VIEW_DESC GetStructuredUavDesc(const RWBufferResource& rwBuffer, uint32_t stride, uint32_t firstElement = 0);

D3D12_SHADER_RESOURCE_VIEW_DESC GetBufferAccelerationStructureSrvDesc(D3D12_GPU_VIRTUAL_ADDRESS address);

struct BufferDesc
{
	uint32_t size;
	bool isRTAcceleration : 1 = false;
	bool initializeToZero : 1 = false;
	LPCWSTR name = L"";
};

struct RWRawBuffer : RWBufferResource
{
	DescriptorHeap::Allocation srvId;
	DescriptorHeap::Allocation uavId;
};

struct RWRawBufferWithCounter : RWRawBuffer
{
	void ResetCounter(ID3D12GraphicsCommandList10* commandList);
};

RWRawBuffer CreateRWRawBuffer(ID3D12Device10* device,
	BufferDesc&& bufferDesc,
	DescriptorHeap& descriptorHeap,
	D3D12_HEAP_TYPE heapType = D3D12_HEAP_TYPE_GPU_UPLOAD);

RWRawBufferWithCounter CreateRWRawBufferWithCounter(ID3D12Device10* device,
	BufferDesc&& bufferDesc,
	DescriptorHeap& descriptorHeap,
	D3D12_HEAP_TYPE heapType = D3D12_HEAP_TYPE_GPU_UPLOAD);

void DestroySafe(RWRawBuffer& rwBuffer);

DescriptorHeap::Allocation CreateSrvOnHeap(DescriptorHeap& descriptorHeap, const BufferResource& buffer);
DescriptorHeap::Allocation CreateUavOnHeap(DescriptorHeap& descriptorHeap, const RWBufferResource& buffer);

DescriptorHeap::Id CreateSrvOnHeap(TemporaryDescriptorHeap& descriptorHeap, const BufferResource& buffer);
DescriptorHeap::Id CreateUavOnHeap(TemporaryDescriptorHeap& descriptorHeap, const RWBufferResource& buffer);

