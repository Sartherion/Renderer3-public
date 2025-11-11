#pragma once
#include "Allocator.h"

namespace D3D
{
	extern uint32_t srvHandleSize;
	extern uint32_t rtvHandleSize;
	extern uint32_t dsvHandleSize;
}

struct DescriptorHeapBase
{
	ComPtr<ID3D12DescriptorHeap> heap;
	OffsetAllocator::Allocator allocator;

	uint32_t elementMaxCount;
	ID3D12Device10* device;

	void Init(ID3D12Device10* device, uint32_t elementMaxCount, D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flag);
};

struct DescriptorHandle 
{
	D3D12_CPU_DESCRIPTOR_HANDLE handle;
	D3D12_GPU_DESCRIPTOR_HANDLE handleGpu;

	DescriptorHandle Offset(int count) const
	{
		return
		{
			.handle = { handle.ptr + count * D3D::srvHandleSize },
			.handleGpu = { handleGpu.ptr + count * D3D::srvHandleSize }
		};
	}

	DescriptorHandle& operator=(const DescriptorHandle&) & = default; //@note: this prevents assignment to temporaries of this type

	operator D3D12_CPU_DESCRIPTOR_HANDLE() const
	{
		return handle;
	}

	operator D3D12_GPU_DESCRIPTOR_HANDLE() const
	{
		return handleGpu;
	}
};

struct RtvHandle
{
	D3D12_CPU_DESCRIPTOR_HANDLE handle;
	
	RtvHandle Offset(int count) const
	{
		return 
		{
			.handle = { handle.ptr + count * D3D::rtvHandleSize }
		};
	}

	RtvHandle& operator=(const RtvHandle&) & = default; //@note: this prevents assignment to temporaries of this type

	operator D3D12_CPU_DESCRIPTOR_HANDLE() const
	{
		return handle;
	}
};

struct DsvHandle
{
	D3D12_CPU_DESCRIPTOR_HANDLE handle;

	DsvHandle Offset(int count) const
	{
		return 
		{
			.handle = { handle.ptr + count * D3D::dsvHandleSize }
		};
	}

	DsvHandle& operator=(const DsvHandle&) & = default; //@note: this prevents assignment to temporaries of this type

	operator D3D12_CPU_DESCRIPTOR_HANDLE() const
	{
		return handle;
	}
};

struct DescriptorHeap : DescriptorHeapBase //@note: using just the term DescriptorHeap means always cbv srv uav heap for our purpose
{
	using Id = DescriptorHeapId;
	inline static constexpr Id InvalidId = DescriptorHeapInvalidId;

	struct Allocation
	{
		static DescriptorHeap* const parentHeapPtr;

		OffsetAllocator::Allocation allocation = { InvalidId };

		operator DescriptorHeap::Id() const
		{
			return Id();
		}

		DescriptorHeap::Id Id(uint32_t index = 0) const
		{
			return allocation.offset + index;
		}

		DescriptorHeap::Id operator[](uint32_t index) const
		{
			return Id(index);
		}

		bool IsValid() const
		{
			return allocation.offset != InvalidId;
		}

		void Free()
		{
			assert(IsValid());
			parentHeapPtr->allocator.free(allocation);
		}
	};

	DescriptorHandle baseHandle;

	void Init(ID3D12Device10* device, uint32_t elementMaxCount, D3D12_DESCRIPTOR_HEAP_FLAGS flag = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);

	Allocation Allocate(uint32_t elementCount = 1);

	DescriptorHandle GetDescriptorHandleAt(uint32_t offset) const;
};

struct RtvHeap : DescriptorHeapBase
{
	struct Allocation
	{
		static RtvHeap* const parentHeapPtr;

		OffsetAllocator::Allocation allocation = { OffsetAllocator::Allocation::NO_SPACE };

		RtvHandle Handle() const
		{
			return parentHeapPtr->GetDescriptorHandleAt(this->allocation.offset);
		}

		bool IsValid() const
		{
			return allocation.offset != OffsetAllocator::Allocation::NO_SPACE;
		}

		void Free()
		{
			assert(IsValid());
			parentHeapPtr->allocator.free(allocation);
		}
	};

	RtvHandle baseHandle;

	void Init(ID3D12Device10* device, uint32_t elementMaxCount);

	Allocation Allocate(uint32_t elementCount = 1);

	RtvHandle GetDescriptorHandleAt(uint32_t offset) const;
};

struct DsvHeap : DescriptorHeapBase
{
	struct Allocation
	{
		static DsvHeap* const parentHeapPtr;

		OffsetAllocator::Allocation allocation = { OffsetAllocator::Allocation::NO_SPACE };

		DsvHandle Handle() const
		{
			return parentHeapPtr->GetDescriptorHandleAt(this->allocation.offset);
		}

		bool IsValid() const
		{
			return allocation.offset != OffsetAllocator::Allocation::NO_SPACE;
		}

		void Free()
		{
			assert(IsValid());
			parentHeapPtr->allocator.free(allocation);
		}
	};

	DsvHandle baseHandle;

	void Init(ID3D12Device10* device, uint32_t elementMaxCount);

	Allocation Allocate(uint32_t elementCount = 1);

	DsvHandle GetDescriptorHandleAt(uint32_t offset) const;
};

namespace D3D
{
	extern DescriptorHeap descriptorHeap;
	extern RtvHeap rtvHeap;
	extern DsvHeap dsvHeap;
}

DescriptorHeap::Allocation CreateSrvOnHeap(DescriptorHeap& descriptorHeap,
	ID3D12Resource1* resource,
	const std::optional<D3D12_SHADER_RESOURCE_VIEW_DESC>& srvDesc = std::nullopt);

DescriptorHeap::Allocation CreateUavOnHeap(DescriptorHeap& descriptorHeap,
	ID3D12Resource1* resource,
	const std::optional<D3D12_UNORDERED_ACCESS_VIEW_DESC>& uavDesc = std::nullopt);

struct TemporaryDescriptorHeap
{
	ChunkAllocator<OffsetAllocator::Allocation> allocator;
	DescriptorHeap* parentHeap;

	void Init(DescriptorHeap& parentHeap, uint32_t chunkSizeBytes = 512, uint32_t initialChunkCount = 1);

	DescriptorHeap::Id Allocate(uint32_t elementCount = 1);

	DescriptorHandle GetDescriptorHandleAt(uint32_t offset) const;
};

DescriptorHeap::Id CreateSrvOnHeap(TemporaryDescriptorHeap& descriptorHeap,
	ID3D12Resource1* resource,
	const std::optional<D3D12_SHADER_RESOURCE_VIEW_DESC>& srvDesc = std::nullopt);

DescriptorHeap::Id CreateUavOnHeap(TemporaryDescriptorHeap& descriptorHeap,
	ID3D12Resource1* resource,
	const std::optional<D3D12_UNORDERED_ACCESS_VIEW_DESC>& uavDesc = std::nullopt);

