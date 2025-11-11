#include "stdafx.h"
#include "BufferMemory.h"

void BufferHeap::Init(ID3D12Device10* device, uint32_t size, LPCWSTR name, D3D12_HEAP_TYPE heapType)
{
	allocator = { size };
	parentBuffer = CreateBufferResource(device, { .size = size, .name = name }, heapType, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	assert((parentBuffer.resource->GetGPUVirtualAddress() & 3)  == 0);  
}

void BufferHeap::WriteRaw(Offset offset, const void* sourcePtr, uint32_t size) const 
{
	assert(offset + size <= parentBuffer.size);
	uint8_t* destinationPtr = (uint8_t*)parentBuffer.cpuPtr + offset;
	std::memcpy(destinationPtr, sourcePtr, size);
}

void ScratchHeap::Init(BufferHeap& parentHeap, uint32_t chunkSizeBytes, uint32_t initialChunkCount)
{
	this->parentHeap = &parentHeap;
	allocator.Init(
		[&parentHeap](uint32_t size)
		{
			return parentHeap.Allocate(size);
		},
		chunkSizeBytes, initialChunkCount);
}

ScratchHeap::Allocation ScratchHeap::Allocate(uint32_t sizeBytes, uint32_t alignmentBytes)
{
	uint32_t alignedSizeBytes = sizeBytes + alignmentBytes;
	auto allocation = allocator.Allocate(alignedSizeBytes);
	uint32_t totalOffset = allocation.chunk.offset + allocation.offset;
	totalOffset = static_cast<uint32_t>(Align(totalOffset, alignmentBytes));
	return
	{
		.offset = totalOffset,
		.size = sizeBytes
	};
}
