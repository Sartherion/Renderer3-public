#pragma once
#include "Buffer.h"
#include "Allocator.h"

#define BufferMemberOffset(buffer, member)\
	(buffer.Offset() + offsetof(typename decltype(buffer)::type, member))


#define WriteBufferMember(buffer, member, data)\
	(buffer.parentHeap->WriteRaw(BufferMemberOffset(buffer, member), &data, sizeof(data)))

struct BufferHeap : AllocatorBase<BufferHeap>
{
	static const uint32_t maximumSupportedAlignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	BufferResource parentBuffer;

	void Init(ID3D12Device10* device, 
	uint32_t size, LPCWSTR name = L"", 
	D3D12_HEAP_TYPE heapType = D3D12_HEAP_TYPE_GPU_UPLOAD);

	void WriteRaw(Offset offset, const void* sourcePtr, uint32_t size) const;
};

template <typename T>
struct PersistentBuffer : BufferHeap::Allocation
{
	using type = T;

	void Write(const T& element, uint32_t elementOffset = 0) const
	{
		allocator->WriteRaw(Offset(elementOffset), &element, sizeof(T));
	}

	void Write(std::span<const T> elements, uint32_t elementOffset = 0) const
	{
		allocator->WriteRaw(Offset(elementOffset), elements.data(), static_cast<uint32_t>(elements.size_bytes()));
	}

	BufferHeap::Offset Offset(uint32_t elementIndex = 0) const
	{
		return offset + elementIndex * sizeof(T);
	}

	D3D12_GPU_VIRTUAL_ADDRESS GPUAddress(uint32_t elementIndex = 0) const
	{
		return Offset(elementIndex) + allocator->parentBuffer.resource->GetGPUVirtualAddress();
	}
};

template <typename T> 
PersistentBuffer<T> CreatePersistentBuffer(BufferHeap& heap, uint32_t elementCount = 1, uint32_t alignment = alignof(T))
{
	alignment = Max(alignment, 4u); //@todo: restriction could be lifted, but for now only allow 4B aligned types
	PersistentBuffer<T> buffer;
	static_cast<BufferHeap::Allocation&>(buffer) = heap.Allocate(elementCount * sizeof(T), alignment);

	return buffer;
}

struct ScratchHeap 
{
	struct Allocation
	{
		BufferHeap::Offset offset;
		uint32_t size; //@todo: size not needed except for assert
	};

	void Init(BufferHeap& parentHeap, uint32_t chunkSizeBytes = 10 * 1024, uint32_t initialChunkCount = 10);
	Allocation Allocate(uint32_t sizeBytes, uint32_t alignmentBytes);

	BufferHeap* parentHeap;
	ChunkAllocator<BufferHeap::Allocation> allocator;
};

template <typename T>
struct TemporaryBuffer : ScratchHeap::Allocation 
{
	using type = T;

	BufferHeap* parentHeap;

	void Write(const T& element, uint32_t elementOffset = 0) 
	{
		parentHeap->WriteRaw(Offset(elementOffset), &element, sizeof(T));
	}

	void Write(std::span<const T> elements, uint32_t elementOffset = 0) 
	{
		parentHeap->WriteRaw(Offset(elementOffset), elements.data(), static_cast<uint32_t>(elements.size_bytes()));
	}

	BufferHeap::Offset Offset(uint32_t element = 0) const
	{
		uint32_t internalOffset = element * sizeof(T);
		assert(size == 0 || internalOffset < size);

		return offset + internalOffset;
	}

	D3D12_GPU_VIRTUAL_ADDRESS GPUAddress(uint32_t elementIndex = 0) const
	{
		return Offset(elementIndex) + parentHeap->parentBuffer.resource->GetGPUVirtualAddress();
	}
};

template <typename T> 
TemporaryBuffer<T> CreateTemporaryBuffer(ScratchHeap& heap, uint32_t elementCount = 1, uint32_t alignment = alignof(T))
{
	alignment = Max(alignment, 4u); //@todo: restriction could be lifted, but for now only allow 4B aligned types
	TemporaryBuffer<T> buffer;
	static_cast<ScratchHeap::Allocation&>(buffer) = heap.Allocate(elementCount * sizeof(T), alignment);
	buffer.parentHeap = heap.parentHeap;

	return buffer;
}

template <typename T>
BufferHeap::Offset WriteTemporaryData(ScratchHeap& heap, std::span<const T> data)
{
	TemporaryBuffer<T> buffer = CreateTemporaryBuffer<T>(heap, static_cast<uint32_t>(data.size()));
	buffer.Write(data);
	
	return buffer.Offset();
}

template <typename T>
BufferHeap::Offset WriteTemporaryData(ScratchHeap& heap, const T& data)
{
	return WriteTemporaryData(heap,  std::span<const T>{&data, 1});
}
