#include "stdafx.h"
#include "DescriptorHeap.h"

DescriptorHeap* const DescriptorHeap::Allocation::parentHeapPtr = &D3D::descriptorHeap;

static ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(ComPtr<ID3D12Device> device, 
	D3D12_DESCRIPTOR_HEAP_TYPE heapType, 
	uint32_t numberOfElements, 
	D3D12_DESCRIPTOR_HEAP_FLAGS heapFlags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE);


void DescriptorHeapBase::Init(ID3D12Device10* device, uint32_t elementMaxCount,	D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flag)
{
	heap = CreateDescriptorHeap(device, type, elementMaxCount, flag);
	allocator = { elementMaxCount };
	this->elementMaxCount = elementMaxCount;
	this->device = device;
}

DescriptorHeap::Allocation DescriptorHeap::Allocate(uint32_t elementCount)
{
	return
	{
		.allocation = allocator.allocate(elementCount),
	};
}

void DescriptorHeap::Init(ID3D12Device10* device, uint32_t elementMaxCount, D3D12_DESCRIPTOR_HEAP_FLAGS flag)
{
	const bool gpuVisible = flag == D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	DescriptorHeapBase::Init(device, elementMaxCount, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, flag);
	baseHandle =
	{
		.handle = heap->GetCPUDescriptorHandleForHeapStart(),
		.handleGpu = gpuVisible ? 
			heap->GetGPUDescriptorHandleForHeapStart() :
			D3D12_GPU_DESCRIPTOR_HANDLE{ UINT64(-1) } //@note: not legal to call GetGPUDescriptorHandleForHeapStart() on a non shader visible heap
	};
}

DescriptorHandle DescriptorHeap::GetDescriptorHandleAt(uint32_t offset) const
{
	return baseHandle.Offset(offset);
}

RtvHeap* const RtvHeap::Allocation::parentHeapPtr = &D3D::rtvHeap;

void RtvHeap::Init(ID3D12Device10* device, uint32_t elementMaxCount)
{
	DescriptorHeapBase::Init(device, elementMaxCount, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
	baseHandle = { .handle = heap->GetCPUDescriptorHandleForHeapStart() };
}

RtvHeap::Allocation RtvHeap::Allocate(uint32_t elementCount)
{
	return
	{
		.allocation = allocator.allocate(elementCount),
	};
}

RtvHandle RtvHeap::GetDescriptorHandleAt(uint32_t offset) const
{
	return baseHandle.Offset(offset);
}

DsvHeap* const DsvHeap::Allocation::parentHeapPtr = &D3D::dsvHeap;

void DsvHeap::Init(ID3D12Device10* device, uint32_t elementMaxCount)
{
	DescriptorHeapBase::Init(device, elementMaxCount, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
	baseHandle = { .handle = heap->GetCPUDescriptorHandleForHeapStart() };
}

DsvHeap::Allocation DsvHeap::Allocate(uint32_t elementCount)
{
	return
	{
		.allocation = allocator.allocate(elementCount),
	};
}

DsvHandle DsvHeap::GetDescriptorHandleAt(uint32_t offset) const
{
	return baseHandle.Offset(offset);
}

DescriptorHeap::Allocation CreateSrvOnHeap(DescriptorHeap& descriptorHeap,
	ID3D12Resource1* resource,
	const std::optional<D3D12_SHADER_RESOURCE_VIEW_DESC>& srvDesc)
{
	DescriptorHeap::Allocation allocation = descriptorHeap.Allocate();
	descriptorHeap.device->CreateShaderResourceView(resource, srvDesc.has_value() ? &srvDesc.value() : nullptr, descriptorHeap.GetDescriptorHandleAt(allocation));
	return allocation;
}

DescriptorHeap::Allocation CreateUavOnHeap(DescriptorHeap& descriptorHeap,
	ID3D12Resource1* resource,
	const std::optional<D3D12_UNORDERED_ACCESS_VIEW_DESC>& uavDesc)
{
	DescriptorHeap::Allocation allocation = descriptorHeap.Allocate();
	descriptorHeap.device->CreateUnorderedAccessView(resource, nullptr, uavDesc.has_value() ? &uavDesc.value() : nullptr, descriptorHeap.GetDescriptorHandleAt(allocation));
	return allocation;
}

DescriptorHeap::Id CreateSrvOnHeap(TemporaryDescriptorHeap& descriptorHeap,
	ID3D12Resource1* resource,
	const std::optional<D3D12_SHADER_RESOURCE_VIEW_DESC>& srvDesc)
{
	DescriptorHeap::Id allocation = descriptorHeap.Allocate();
	descriptorHeap.parentHeap->device->CreateShaderResourceView(resource, srvDesc.has_value() ? &srvDesc.value() : nullptr, descriptorHeap.GetDescriptorHandleAt(allocation));
	return allocation;
}

DescriptorHeap::Id CreateUavOnHeap(TemporaryDescriptorHeap& descriptorHeap,
	ID3D12Resource1* resource,
	const std::optional<D3D12_UNORDERED_ACCESS_VIEW_DESC>& uavDesc)
{
	DescriptorHeap::Id allocation = descriptorHeap.Allocate();
	descriptorHeap.parentHeap->device->CreateUnorderedAccessView(resource, nullptr, uavDesc.has_value() ? &uavDesc.value() : nullptr, descriptorHeap.GetDescriptorHandleAt(allocation));
	return allocation;
}

void TemporaryDescriptorHeap::Init(DescriptorHeap& parentHeap, uint32_t chunkSizeBytes, uint32_t initialChunkCount)
{
	this->parentHeap = &parentHeap;
	allocator.Init(
		[&parentHeap](uint32_t elementCount)
		{
			return parentHeap.allocator.allocate(elementCount);
		},
		chunkSizeBytes,
		initialChunkCount
	);
}

DescriptorHeap::Id TemporaryDescriptorHeap::Allocate(uint32_t elementCount)
{
	auto allocation = allocator.Allocate(elementCount);
	assert(allocation.chunk.offset != uint32_t(-1));

	return allocation.chunk.offset + allocation.offset;
}

DescriptorHandle TemporaryDescriptorHeap::GetDescriptorHandleAt(uint32_t offset) const
{
	return parentHeap->baseHandle.Offset(offset);
}

ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(ComPtr<ID3D12Device> device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, uint32_t numberOfElements, D3D12_DESCRIPTOR_HEAP_FLAGS heapFlags)
{
	ComPtr<ID3D12DescriptorHeap> heap;
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc;
	heapDesc.Type = heapType;
	heapDesc.Flags = heapFlags;
	heapDesc.NumDescriptors = numberOfElements;
	heapDesc.NodeMask = 0;
	CheckForErrors(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&heap)));
	return heap;
}
