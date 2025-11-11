#include "stdafx.h"
#include "Buffer.h"

#include "D3DUtility.h"
#include "Frame.h"

static constexpr uint32_t CalculateConstantBufferSize(uint32_t byteSize)
{
	return (byteSize + 255) & ~255;
}

static uint32_t GetFormatStrideBytes(DXGI_FORMAT format)
{
	return static_cast<uint32_t>(DirectX::BitsPerPixel(format) / 8);
}

static RWRawBuffer CreateRWRawBuffer(ID3D12Device10* device,
	BufferDesc&& bufferDesc,
	DescriptorHeap& descriptorHeap,
	bool hasCounter,
	D3D12_HEAP_TYPE heapType);

template <bool isReadWrite = false> auto CreateBufferResource(ID3D12Device10* device,
	const BufferDesc& bufferDesc,
	D3D12_HEAP_TYPE heapType = D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAGS additionalFlags = D3D12_RESOURCE_FLAG_NONE)
{
	const bool isRTAccelaration = bufferDesc.isRTAcceleration;

	D3D12_RESOURCE_DESC1 resourceDesc = {};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	resourceDesc.MipLevels = 1;
	resourceDesc.Alignment = 0; //@note: buffers must have alignment of 65K.
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.Flags = (isReadWrite ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE)
		| (isRTAccelaration ? D3D12_RESOURCE_FLAG_RAYTRACING_ACCELERATION_STRUCTURE : D3D12_RESOURCE_FLAG_NONE)
		| additionalFlags;
	resourceDesc.Width = bufferDesc.size;
	resourceDesc.Height = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;

	D3D12_HEAP_PROPERTIES bufferHeapProperties;
	bufferHeapProperties.Type = heapType;
	bufferHeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	bufferHeapProperties.CreationNodeMask = 0;
	bufferHeapProperties.VisibleNodeMask = 0;
	bufferHeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	ComPtr<ID3D12Resource1> buffer;
	device->CreateCommittedResource3(&bufferHeapProperties, bufferDesc.initializeToZero ? D3D12_HEAP_FLAG_NONE : D3D12_HEAP_FLAG_CREATE_NOT_ZEROED, &resourceDesc,  D3D12_BARRIER_LAYOUT_UNDEFINED, nullptr, nullptr, 0, nullptr, IID_PPV_ARGS(&buffer));
	buffer->SetName(bufferDesc.name);

	void* cpuPtr = nullptr;
	if (heapType == D3D12_HEAP_TYPE_GPU_UPLOAD)
	{
		buffer->Map(0, nullptr, &cpuPtr);
	}

	if constexpr (isReadWrite)
	{
		return RWBufferResource{ std::move(buffer), cpuPtr, bufferDesc.size };
	}
	else
	{
		return BufferResource{ std::move(buffer), cpuPtr, bufferDesc.size };
	}
}

RWBufferResource CreateRWBufferResource(ID3D12Device10* device,
	BufferDesc&& bufferDesc,
	D3D12_HEAP_TYPE heapType)
{
	return CreateBufferResource<true>(device, std::move(bufferDesc), heapType);
}

BufferResource CreateBufferResource(ID3D12Device10* device,
	BufferDesc&& bufferDesc,
	D3D12_HEAP_TYPE heapType, D3D12_RESOURCE_FLAGS additionalFlags)
{
	return CreateBufferResource<false>(device, std::move(bufferDesc), heapType, additionalFlags); 
}

BufferResource CreateConstantBufferResource(ID3D12Device10* device,
	BufferDesc&& bufferDesc,
	D3D12_HEAP_TYPE heapType)
{
	BufferDesc resizedBufferDesc = bufferDesc;
	resizedBufferDesc.size = CalculateConstantBufferSize(bufferDesc.size);
	return CreateBufferResource(device, resizedBufferDesc, heapType);
}

D3D12_SHADER_RESOURCE_VIEW_DESC GetBufferSrvDesc(const D3D12_BUFFER_SRV& srvDescBuffer, DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = format;
	srvDesc.Buffer = srvDescBuffer;
	return srvDesc;
}


D3D12_UNORDERED_ACCESS_VIEW_DESC GetBufferUavDesc(const D3D12_BUFFER_UAV& uavDescBuffer, DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN)
{
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Format = format;
	uavDesc.Buffer = uavDescBuffer;
	return uavDesc;
}

D3D12_SHADER_RESOURCE_VIEW_DESC GetBufferAccelerationStructureSrvDesc(D3D12_GPU_VIRTUAL_ADDRESS address)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.RaytracingAccelerationStructure.Location = address;
	return srvDesc;
}

D3D12_SHADER_RESOURCE_VIEW_DESC GetRawSrvDesc(const BufferResource& buffer, uint32_t firstElement)
{
	return GetBufferSrvDesc({ .FirstElement = firstElement,
		.NumElements = buffer.size / 4 - firstElement,
		.Flags = D3D12_BUFFER_SRV_FLAG_RAW }, DXGI_FORMAT_R32_TYPELESS);
}

D3D12_SHADER_RESOURCE_VIEW_DESC GetTypedSrvDesc(const BufferResource& buffer, DXGI_FORMAT format, uint32_t firstElement)
{
	return GetBufferSrvDesc({ .FirstElement = firstElement,
		.NumElements = buffer.size / GetFormatStrideBytes(format) - firstElement},
		format);
}

D3D12_SHADER_RESOURCE_VIEW_DESC GetStructuredSrvDesc(const BufferResource& buffer, uint32_t stride, uint32_t firstElement)
{
	return GetBufferSrvDesc({ .FirstElement = firstElement,
		.NumElements = buffer.size / stride - firstElement,
		.StructureByteStride = stride },
		DXGI_FORMAT_UNKNOWN);
}

D3D12_UNORDERED_ACCESS_VIEW_DESC GetRawUavDesc(const RWBufferResource& rwBuffer, uint32_t firstElement)
{
	return GetBufferUavDesc({ .FirstElement = firstElement,
		.NumElements = rwBuffer.size / 4 - firstElement,
		.Flags = D3D12_BUFFER_UAV_FLAG_RAW }, DXGI_FORMAT_R32_TYPELESS);
}

D3D12_UNORDERED_ACCESS_VIEW_DESC GetTypedUavDesc(const RWBufferResource& rwBuffer, DXGI_FORMAT format, uint32_t firstElement)
{
	return GetBufferUavDesc({ .FirstElement = firstElement,
		.NumElements = rwBuffer.size / GetFormatStrideBytes(format) - firstElement },
		format);
}

D3D12_UNORDERED_ACCESS_VIEW_DESC GetStructuredUavDesc(const RWBufferResource& rwBuffer, uint32_t stride, uint32_t firstElement)
{
	return GetBufferUavDesc({ .FirstElement = firstElement,
		.NumElements = rwBuffer.size / stride - firstElement,
		.StructureByteStride = stride },
		DXGI_FORMAT_UNKNOWN);
}

RWRawBuffer CreateRWRawBuffer(ID3D12Device10* device, BufferDesc&& bufferDesc, DescriptorHeap& descriptorHeap, D3D12_HEAP_TYPE heapType)
{
	return CreateRWRawBuffer(device, std::move(bufferDesc), descriptorHeap, false, heapType);
}

RWRawBufferWithCounter CreateRWRawBufferWithCounter(ID3D12Device10* device, BufferDesc&& bufferDesc, DescriptorHeap& descriptorHeap, D3D12_HEAP_TYPE heapType)
{
	RWRawBufferWithCounter buffer;
	static_cast<RWRawBuffer&>(buffer) = CreateRWRawBuffer(device, std::move(bufferDesc), descriptorHeap, true, heapType);

	return buffer;
}

void DestroySafe(RWRawBuffer& rwBuffer)
{
	Frame::SafeRelease(rwBuffer.srvId);
	Frame::SafeRelease(rwBuffer.uavId);

	rwBuffer.resource->Unmap(0, nullptr);
	Frame::SafeRelease(std::move(rwBuffer.resource));
	rwBuffer.resource = nullptr;
}

DescriptorHeap::Allocation CreateSrvOnHeap(DescriptorHeap& descriptorHeap, const BufferResource& buffer)
{
	return CreateSrvOnHeap(descriptorHeap, buffer.resource.Get(), GetRawSrvDesc(buffer));
}

DescriptorHeap::Allocation CreateUavOnHeap(DescriptorHeap& descriptorHeap, const RWBufferResource& buffer)
{
	return CreateUavOnHeap(descriptorHeap, buffer.resource.Get(), GetRawUavDesc(buffer));
}

DescriptorHeap::Id CreateSrvOnHeap(TemporaryDescriptorHeap& descriptorHeap, const BufferResource& buffer)
{
	return CreateSrvOnHeap(descriptorHeap, buffer.resource.Get(), GetRawSrvDesc(buffer));
}

DescriptorHeap::Id CreateUavOnHeap(TemporaryDescriptorHeap& descriptorHeap, const RWBufferResource& buffer)
{
	return CreateUavOnHeap(descriptorHeap, buffer.resource.Get(), GetRawUavDesc(buffer));
}

//@todo: properly respect bufferDesc.alignment for buffer in buffer + counter resource
RWRawBuffer CreateRWRawBuffer(ID3D12Device10* device, BufferDesc&& bufferDesc, DescriptorHeap& descriptorHeap, bool hasCounter, D3D12_HEAP_TYPE heapType)
{
	RWRawBuffer buffer;
	BufferDesc counterBufferDesc = bufferDesc;
	counterBufferDesc.size = bufferDesc.size + 4 * sizeof(uint32_t);  //@note: need only 4B extra, but srv first element offset can only be multiple of 16B!
	static_cast<RWBufferResource&>(buffer) = CreateRWBufferResource(device, hasCounter ? std::move(counterBufferDesc) : std::move(bufferDesc), heapType);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = bufferDesc.isRTAcceleration ? GetBufferAccelerationStructureSrvDesc(buffer.resource->GetGPUVirtualAddress()) : GetRawSrvDesc(buffer, hasCounter ? 4 : 0);
	buffer.srvId = CreateSrvOnHeap(descriptorHeap, bufferDesc.isRTAcceleration ? nullptr : buffer.resource.Get(), srvDesc);

	buffer.uavId = CreateUavOnHeap(descriptorHeap, buffer);

	return buffer;
}

D3D12_BUFFER_BARRIER BufferResource::Barrier(ResourceState from, ResourceState to) const
{
		D3D12_BUFFER_BARRIER barrier;

		BarrierFields fromFields = GetBarrierFieldsFor(from);
		BarrierFields toFields = GetBarrierFieldsFor(to);

		barrier.SyncBefore = fromFields.sync;
		barrier.SyncAfter = toFields.sync;
		barrier.AccessBefore = fromFields.access;
		barrier.AccessAfter = toFields.access;
		barrier.Offset = 0;
		barrier.Size = UINT64_MAX;
		barrier.pResource = resource.Get();

		return barrier;
}

void RWRawBufferWithCounter::ResetCounter(ID3D12GraphicsCommandList10* commandList)
{
	WriteImmediateValue(commandList, resource->GetGPUVirtualAddress(), 0);
}
