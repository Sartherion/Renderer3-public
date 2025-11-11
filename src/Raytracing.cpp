#include "stdafx.h"
#include "Raytracing.h"

#include "Buffer.h"
#include "Frame.h"

void BuildAccelerationStructure(BufferResource& inoutAccelerationStructureBuffer,
	ID3D12Device10* device,
	ID3D12GraphicsCommandList10* commandList,
	const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& inputs,
	const RWBufferResource& scratchBuffer, LPCWSTR name)
{
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuildInfo;
	device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuildInfo);

	assert(scratchBuffer.size >= prebuildInfo.ScratchDataSizeInBytes);

	if (inoutAccelerationStructureBuffer.size < prebuildInfo.ResultDataMaxSizeInBytes)
	{
		Frame::SafeRelease(std::move(inoutAccelerationStructureBuffer.resource));
		inoutAccelerationStructureBuffer = CreateRWBufferResource(device,
			{ .size = static_cast<uint32_t>(prebuildInfo.ResultDataMaxSizeInBytes), .isRTAcceleration = true, .name = name },
			D3D12_HEAP_TYPE_DEFAULT);
	}

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC desc = {};
	desc.Inputs = inputs;
	desc.ScratchAccelerationStructureData = scratchBuffer.resource->GetGPUVirtualAddress();
	desc.DestAccelerationStructureData = inoutAccelerationStructureBuffer.resource->GetGPUVirtualAddress();
	commandList->BuildRaytracingAccelerationStructure(&desc, 0, nullptr);
}

void RefitAccelerationStructure(BufferResource& accelerationStructure,
	ID3D12GraphicsCommandList10* commandList,
	const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& inputs,
	const RWBufferResource& scratchBuffer)
{
	auto modifiedInputs = inputs;
	modifiedInputs.Flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE | D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC desc = {};
	desc.Inputs = modifiedInputs;
	desc.SourceAccelerationStructureData = desc.DestAccelerationStructureData = accelerationStructure.resource->GetGPUVirtualAddress();
	desc.ScratchAccelerationStructureData = scratchBuffer.resource->GetGPUVirtualAddress();
	commandList->BuildRaytracingAccelerationStructure(&desc, 0, nullptr);
}
