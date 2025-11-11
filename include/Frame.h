#pragma once
#include "BufferMemory.h"
#include "DescriptorHeap.h"

namespace Frame
{
	constexpr uint32_t framesInFlightCount = 2;
	constexpr uint32_t cpuMemoryChunkSize = 1024 * 1024;

	inline struct TimingData
	{
		uint64_t frameId;
		double elapsedTimeMs;
		float deltaTimeMs;
		float averageDeltaTimeMs;
	} timingData;

	inline struct HistoryData
	{
		float* frameTime;
		uint32_t frameCount;
		uint32_t frameIndex;
	} historyData;

	struct FrameData
	{
		uint32_t index = 0;
		ComPtr<ID3D12CommandAllocator> commandAllocator;
		uint64_t fenceWaitValue;

		LinearAllocator cpuMemory;
		ScratchHeap gpuMemory;
		TemporaryDescriptorHeap descriptorHeap;

		std::vector<ComPtr<ID3D12Resource1>> releasedResources;
		std::vector<ComPtr<ID3D12PipelineState>> releasedPsos;
		std::vector<BufferHeap::Allocation> releasedBufferHeapAllocations;
		std::vector<DescriptorHeap::Allocation> releasedDescriptorHeapAllocations;
		std::vector<RtvHeap::Allocation> releasedRtvHeapAllocations;
		std::vector<DsvHeap::Allocation> releasedDsvHeapAllocations;
	};

	inline FrameData* current;

	void Init(ID3D12Device10* device, BufferHeap& parentBufferHeap, DescriptorHeap& parentDescriptorHeap);

	void Begin();

	void End(ID3D12CommandQueue* commandQueue, ID3D12GraphicsCommandList10* commandListm, float forceCpuFrameTime = 0.0f);

	void FlushCommandQueue();

	void SafeRelease(ComPtr<ID3D12Resource1>&& resource);
	void SafeRelease(ComPtr<ID3D12PipelineState>&& pso);
	void SafeRelease(BufferHeap::Allocation& bufferHeapAllocation);
	void SafeRelease(DescriptorHeap::Allocation& descriptorHeapAllocation);
	void SafeRelease(RtvHeap::Allocation& rtvHeapAllocation);
	void SafeRelease(DsvHeap::Allocation& dsvHeapAllocation);
}

template <typename T>
struct FrameBuffered
{
	T data[Frame::framesInFlightCount];

	T* operator->()
	{
		return &Current();
	}

	const T* operator->() const
	{
		return &Current();
	}

	T& operator*()
	{
		return Current();
	}

	const T& operator*() const
	{
		return Current();
	}

	T& Current()
	{
		return data[Frame::current->index];
	}

	const T& Current() const
	{
		return Current();
	}

	// begin and end for range-based for iteration
	T* begin()
	{
		return data;
	}

	const T* begin() const
	{
		return data;
	}

	T* end()
	{
		return &data[Frame::framesInFlightCount];
	}

	const T* end() const
	{
		return &data[Frame::framesInFlightCount];
	}
};
