#include "stdafx.h"
#include "Frame.h"

static void WaitForFenceValue(ID3D12Fence* fence, uint64_t value);

namespace Frame
{
	static FrameData frameResources[framesInFlightCount];
	static ComPtr<ID3D12Fence> fence;
	static uint32_t currentFrameCount = 0;

	static void UpdateTimingData(float forceCpuFrameTime);
	static std::chrono::high_resolution_clock clock;
	static std::chrono::steady_clock::time_point mCreationTime;
	static std::chrono::steady_clock::time_point mBeginFrameTime{};
	static std::chrono::steady_clock::time_point mEndFrameTime{};

	static Average<float> averageFrameTime{ 60 };

	void Init(ID3D12Device10* device, BufferHeap& parentBufferHeap, DescriptorHeap& parentDescriptorHeap)
	{
		for (uint32_t i = 0; i < framesInFlightCount; i++)
		{
			FrameData& frame = frameResources[i];
			CheckForErrors(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&frame.commandAllocator)));
			frame.fenceWaitValue = 0;
			frame.cpuMemory.Init(cpuMemoryChunkSize);
			frame.gpuMemory.Init(parentBufferHeap, 1024 * 1024);
			frame.descriptorHeap.Init(parentDescriptorHeap);
		}

		historyData.frameTime = averageFrameTime.sampleHistory.data();
		historyData.frameCount = static_cast<uint32_t>(averageFrameTime.sampleHistory.size());

		current = &frameResources[0];

		CheckForErrors(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));

		mCreationTime = clock.now();
	}

	void FlushCommandQueue()
	{
		WaitForFenceValue(fence.Get(), currentFrameCount);
	}
	void Begin()
	{
		mBeginFrameTime = clock.now();
	}

	void End(ID3D12CommandQueue* commandQueue, ID3D12GraphicsCommandList10* commandList, float forceCpuFrameTime)
	{
		current->fenceWaitValue = ++currentFrameCount;
		CheckForErrors(commandQueue->Signal(fence.Get(), current->fenceWaitValue));

		uint32_t currentIndex = currentFrameCount % framesInFlightCount;
		current = &frameResources[currentIndex];
		current->index = currentIndex;

		WaitForFenceValue(fence.Get(), current->fenceWaitValue);

		//now it is safe to release this frame's resources
		current->releasedResources.clear();
		current->releasedPsos.clear();

		for (auto& allocation : current->releasedBufferHeapAllocations)
		{
			allocation.Free();
		}
		current->releasedBufferHeapAllocations.clear();

		for (auto& allocation : current->releasedDescriptorHeapAllocations)
		{
			allocation.Free();
		}
		current->releasedDescriptorHeapAllocations.clear();

		for (auto& allocation : current->releasedRtvHeapAllocations)
		{
			allocation.Free();
		}
		current->releasedRtvHeapAllocations.clear();

		for (auto& allocation : current->releasedDsvHeapAllocations)
		{
			allocation.Free();
		}
		current->releasedDsvHeapAllocations.clear();

		current->commandAllocator->Reset();
		current->cpuMemory.allocator.Reset();
		current->gpuMemory.allocator.Reset();
		current->descriptorHeap.allocator.Reset();

		commandList->Reset(Frame::current->commandAllocator.Get(), nullptr);

		UpdateTimingData(forceCpuFrameTime);
	}

	void UpdateTimingData(float forceCpuFrameTime)
	{
		mEndFrameTime = clock.now();

		//busy wait until desired cpuFrameTime is reached
		float deltaTime = (mEndFrameTime - mBeginFrameTime).count() * 1e-6f;
		while (deltaTime < forceCpuFrameTime)
		{
			mEndFrameTime = clock.now();
			deltaTime = (mEndFrameTime - mBeginFrameTime).count() * 1e-6f;
		}

		timingData.frameId++;
		timingData.deltaTimeMs = deltaTime;
		averageFrameTime.AddSample(timingData.deltaTimeMs);
		timingData.elapsedTimeMs = (mEndFrameTime - mCreationTime).count() * 1e-6;
		timingData.averageDeltaTimeMs = averageFrameTime.GetLatestAverage();
		historyData.frameIndex = averageFrameTime.counter;
	}

	void SafeRelease(ComPtr<ID3D12Resource1>&& resource)
	{
		if (resource)
		{
			current->releasedResources.push_back(std::move(resource));
		}
	}

	void SafeRelease(ComPtr<ID3D12PipelineState>&& pso)
	{
		if (pso)
		{
			current->releasedPsos.push_back(std::move(pso));
		}
	}

	void SafeRelease(BufferHeap::Allocation& bufferHeapAllocation)
	{
		if (bufferHeapAllocation.IsValid())
		{
			current->releasedBufferHeapAllocations.push_back(bufferHeapAllocation);
			bufferHeapAllocation.offset = BufferHeap::InvalidOffset;
		}
	}

	void SafeRelease(DescriptorHeap::Allocation& descriptorHeapAllocation)
	{
		if (descriptorHeapAllocation.IsValid())
		{
			current->releasedDescriptorHeapAllocations.push_back(descriptorHeapAllocation);
			descriptorHeapAllocation = {};
		}
	}

	void SafeRelease(RtvHeap::Allocation& rtvHeapAllocation)
	{
		if (rtvHeapAllocation.IsValid())
		{
			current->releasedRtvHeapAllocations.push_back(rtvHeapAllocation);
			rtvHeapAllocation = {};
		}
	}

	void SafeRelease(DsvHeap::Allocation& dsvHeapAllocation)
	{
		if (dsvHeapAllocation.IsValid())
		{
			current->releasedDsvHeapAllocations.push_back(dsvHeapAllocation);
			dsvHeapAllocation = {};
		}
	}
}

void WaitForFenceValue(ID3D12Fence* fence, uint64_t value)
{
	if (fence->GetCompletedValue() < value)
	{
		HANDLE eventHandle = CreateEvent(nullptr, false, false, nullptr);
		CheckForErrors(fence->SetEventOnCompletion(value, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}
}