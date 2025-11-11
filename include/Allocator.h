#pragma once
#include "MathHelpers.h"

template <typename T>
struct ChunkAllocator
{
	std::function<T(uint32_t)> allocationFunction; 
	std::vector<T> allChunks;
	uint32_t chunkSize;

	struct Allocation 
	{
		T chunk; 
		uint32_t offset = 0; // offset in current chunk
	};

	struct Marker
	{
		int chunkIndex = 0;
		uint32_t offset = 0;
	};

	Marker current;

	template <typename S>
	void Init(S allocationFunction, uint32_t chunkSize, uint32_t initialChunkCount);
	Allocation Allocate(uint32_t size);
	void Reset(const Marker& marker = {});

	Marker GetMarker() const;

	template <typename S>
	void Destroy(S freeFunction);

	uint32_t GetReservedMemoryBytes() const
	{
		return static_cast<uint32_t>(allChunks.size()) * chunkSize;
	}
};

template<typename T>
template<typename S>
inline void ChunkAllocator<T>::Init(S allocationFunction, uint32_t chunkSize, uint32_t initialChunkCount)
{
	this->allocationFunction = allocationFunction;
	this->chunkSize = chunkSize;
	Reset();

	allChunks.reserve(initialChunkCount);
	for (uint32_t i = 0; i < initialChunkCount; i++)
	{
		allChunks.push_back(allocationFunction(chunkSize));
	}
}

template<typename T>
inline ChunkAllocator<T>::Allocation ChunkAllocator<T>::Allocate(uint32_t size)
{
	assert(size <= chunkSize); 

	if (current.offset + size >= chunkSize) // not enough free space in current chunk for allocation
	{
		current.chunkIndex++;
		current.offset = 0;

		if (current.chunkIndex == allChunks.size()) // if no chunks are free, allocate a new chunk
		{
			allChunks.push_back(allocationFunction(chunkSize));
		}
	}
	
	Allocation allocation =
	{
		.chunk = allChunks[current.chunkIndex],
		.offset = current.offset
	};
	current.offset += size;
	return allocation;
}

template<typename T>
inline void ChunkAllocator<T>::Reset(const Marker& marker)
{
	current = marker;
}

template<typename T>
template<typename S>
inline void ChunkAllocator<T>::Destroy(S freeFunction)
{
	for (auto& chunk : allChunks)
	{
		freeFunction(chunk);
	}
}

template<typename T>
inline ChunkAllocator<T>::Marker ChunkAllocator<T>::GetMarker() const
{
	return current;
}


template<size_t pageSize>
struct PoolAllocator
{
	struct Page
	{
		union
		{
			uint8_t data[pageSize];
			uint32_t nextFreeOffset;
		};
	};

	Page* pages = nullptr;
	uint32_t elementCount;

	uint32_t freeListHeadOffset;

	void* Allocate();

	template<typename T> 
	T* Allocate();

	void Free(void* allocation);

	template<typename T>
	void Free(T* ptr);

	void Init(uint32_t elementCount);

	void Reset();

	void Destroy();
};


template<size_t pageSize>
inline void* PoolAllocator<pageSize>::Allocate()
{
	Page& nextFreePage = pages[freeListHeadOffset];
	assert(nextFreePage.nextFreeOffset != uint32_t(-1)); 
	freeListHeadOffset = nextFreePage.nextFreeOffset;
	
	return (void*)nextFreePage.data;
}

template<size_t pageSize>
inline void PoolAllocator<pageSize>::Free(void* ptr)
{
	Page* page = (Page*)ptr;
	page->nextFreeOffset = freeListHeadOffset;
	freeListHeadOffset = static_cast<uint32_t>(page - pages);
}

template<size_t pageSize>
inline void PoolAllocator<pageSize>::Init(uint32_t elementCount)
{
	pages = (Page*)malloc(elementCount * pageSize); 
	this->elementCount = elementCount;
	Reset();
}

template<size_t pageSize>
inline void PoolAllocator<pageSize>::Reset()
{
	freeListHeadOffset = 0;
	for (uint32_t i = 1; i < elementCount; i++)
	{
		pages[i - 1].nextFreeOffset = i;
	}
	pages[elementCount - 1].nextFreeOffset = uint32_t(-1);
}

template<size_t pageSize>
inline void PoolAllocator<pageSize>::Destroy()
{
	free(pages);
}

template<size_t pageSize>
template<typename T>
inline T* PoolAllocator<pageSize>::Allocate() 
{
	static_assert(sizeof(T) <= pageSize);

	void* rawMemory = Allocate();
	return new(rawMemory) T; 
	
}

template<size_t pageSize>
template<typename T>
inline void PoolAllocator<pageSize>::Free(T* ptr)
{
	ptr->~T();
	Free((void*)ptr);
}

struct LinearAllocator
{
	//@todo: to properly support aligned allocations, the chunks need to use aligned allocations as well with an alignment equal to a maximum supported alignment
	ChunkAllocator<uint8_t*> allocator;

	struct Diagnosis
	{
		uint32_t maxSizeInUseByte = 0;
		void Update(const LinearAllocator& allocator)
		{
			const auto& chunkAllocator = allocator.allocator;
			uint32_t sizeInUseByte = chunkAllocator.current.chunkIndex * chunkAllocator.chunkSize + chunkAllocator.current.offset;
			maxSizeInUseByte = Max(maxSizeInUseByte, sizeInUseByte);
		}
	} diagnosis;

	using Marker = decltype(allocator)::Marker;

	void Init(uint32_t chunkSize, uint32_t initialChunkCount = 10)
	{
		diagnosis = {};
		allocator.Init(
			[](uint32_t size)
			{
				return (uint8_t*)malloc(size);
			},
			chunkSize,
			initialChunkCount
		);
	}

	void* AllocateRaw(uint32_t sizeBytes, uint32_t alignBytes = 1)
	{
		uint32_t worstCaseSize = sizeBytes + alignBytes - 1;
		auto allocation = allocator.Allocate(worstCaseSize);
		diagnosis.Update(*this); 

		uintptr_t address = (uintptr_t)allocation.chunk + allocation.offset;
		return (void*)Align(address, alignBytes);
	}

	template<typename T>
	T* Allocate(uint32_t elementCount = 1) 
	{
		//static_assert(std::is_implicit_lifetime<T>_v); //@note: C++23 feature
		T* result = (T*)AllocateRaw(elementCount * sizeof(T), alignof(T));
		for (uint32_t i = 0; i < elementCount; i++)
		{
			result[i] = {};
		}

		return result;
	}

	void Destroy()
	{
		allocator.Destroy(free);
	}
};

template <typename T>
T* WriteTemporaryData(LinearAllocator& allocator, std::span<const T> data)
{
	T* address = allocator.Allocate<T>(static_cast<uint32_t>(data.size()));
	std::memcpy(address, data.data(), data.size_bytes());
	
	return address;
}

template <typename T>
T* WriteTemporaryData(LinearAllocator& allocator, const T& data)
{
	return WriteTemporaryData(allocator, std::span<const T>{&data, 1});
}

struct StackContext;
struct StackAllocator : private LinearAllocator //@note: the sole purpose of this class is to prevent allocations methods of LinearAllocator to be used directly from StackAllocator, instead having to use the StackContext
{
	friend StackContext;

	void Init(uint32_t chunkSize, uint32_t initialChunkCount = 10)
	{
		LinearAllocator::Init(chunkSize, initialChunkCount);
	}

	void Destroy()
	{
		LinearAllocator::Destroy();
	}
	
	void Reset()
	{
		allocator.Reset();
	}

	//diagnosis helper
	uint32_t GetMaxUsageBytes() const
	{
		return diagnosis.maxSizeInUseByte;
	}

	uint32_t GetReservedMemoryBytes() const
	{
		return allocator.GetReservedMemoryBytes();
	}
};

namespace D3D
{
	extern StackAllocator stackAllocator;
}

//A local variable of type StackContext can be defined in a scope to automatically reset all allocations from the linear allocator, when leaving the scope in which the stack context was defined
struct StackContext
{
	StackAllocator& allocator; 
	StackAllocator::Marker marker;

	StackContext(StackAllocator& allocator = D3D::stackAllocator) : allocator{ allocator }, marker{ allocator.allocator.GetMarker() } {}

	~StackContext()
	{
		allocator.allocator.Reset(marker);
	}

	void* AllocateRaw(uint32_t sizeBytes, uint32_t alignBytes = 1)
	{
		return allocator.AllocateRaw(sizeBytes, alignBytes);
	}
	
	template<typename T>
	T* Allocate(uint32_t elementCount = 1) //@note: do not use for types which rely on meta data being initialized or which have a non-triavial destructor
	{
		return allocator.Allocate<T>(elementCount);
	}
};


template <typename T>
struct AllocatorBase
{
	using Offset = BufferHeapOffset;
	inline static const Offset InvalidOffset = BufferHeapInvalidOffset;

	struct Allocation
	{
		T* allocator;
		OffsetAllocator::Allocation rawAllocation;
		Offset offset = InvalidOffset;

		void Free()
		{
			if (IsValid())
			{
				allocator->allocator.free(rawAllocation);
				offset = InvalidOffset;
			}
		}

		uint32_t Size() const
		{
			const uint32_t rawAllocationSize = allocator->allocator.allocationSize(rawAllocation);
			return rawAllocationSize - (offset - rawAllocation.offset);
		}

		bool IsValid() const
		{
			return offset != InvalidOffset;
		}
	};

	OffsetAllocator::Allocator allocator;

	Allocation Allocate(uint32_t size, uint32_t alignment = 4)
	{
		size += alignment;
		Allocation allocation = { .allocator = static_cast<T*>(this), .rawAllocation = allocator.allocate(size) }; 

		allocation.offset = static_cast<uint32_t>(Align(allocation.rawAllocation.offset, alignment));

		return allocation;
	}
};

struct PersistentAllocator : AllocatorBase<PersistentAllocator>
{
	uint32_t totalSize;
	//@todo: to properly support aligned allocations, the chunks need to use aligned allocations as well with an alignment equal to a maximum supported alignment
	void* Ptr(PersistentAllocator::Allocation& allocation)
	{
		return &(static_cast<uint8_t*>(rawMemory)[allocation.offset]);
	}

	const void* Ptr(const PersistentAllocator::Allocation& allocation)
	{
		return &(static_cast<const uint8_t*>(rawMemory)[allocation.offset]);
	}

	void* rawMemory;

	void Init(uint32_t size)
	{
		totalSize = size;
		allocator = { size };
		rawMemory = malloc(size);
	}
};

inline void* GetAllocationPtr(PersistentAllocator::Allocation& allocation)
{
	void* memory = allocation.allocator->rawMemory;
	return &(static_cast<uint8_t*>(memory)[allocation.offset]);
}

inline const void* GetAllocationPtr(const PersistentAllocator::Allocation& allocation)
{
	const void* memory = allocation.allocator->rawMemory;
	return &(static_cast<const uint8_t*>(memory)[allocation.offset]);
}

template <typename T> 
struct PersistentMemory
{
	using type = T;
	PersistentAllocator::Allocation allocation;

	T& Get(uint32_t elementIndex = 0)
	{
		T* ptr = static_cast<T*>(GetAllocationPtr(allocation)); 
		return ptr[elementIndex];
	}
	
	const T& Get(uint32_t elementIndex = 0) const
	{
		const T* ptr = static_cast<const T*>(GetAllocationPtr(allocation)); 
		return ptr[elementIndex];
	}

	const uint32_t Count() const
	{
		return allocation.Size() / sizeof(T);
	}
	
	void Free()
	{
		allocation.Free();
	}
};

template<typename T>
inline PersistentMemory<T> AllocatePersistentMemory(PersistentAllocator& allocator, uint32_t elementCount = 1)
{
	PersistentAllocator::Allocation allocation = allocator.Allocate(elementCount * sizeof(T), alignof(T));

	return { .allocation = allocation };
}

