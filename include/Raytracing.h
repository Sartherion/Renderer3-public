#pragma once

struct RWBufferResource;
struct BufferResource;

void BuildAccelerationStructure(BufferResource& inoutAccelerationStructureBuffer,
	ID3D12Device10* device,
	ID3D12GraphicsCommandList10* commandList,
	const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& inputs,
	const RWBufferResource& scratchBuffer,
	LPCWSTR name = L"Acceleration Structure");

void RefitAccelerationStructure(BufferResource& accelerationStructureBuffer,
	ID3D12GraphicsCommandList10* commandList,
	const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& inputs,
	const RWBufferResource& scratchBuffer);


inline D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS GetBlasInputs(std::span<const D3D12_RAYTRACING_GEOMETRY_DESC> geometryDescs,
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE)
{
	return
	{
		.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL,
		.Flags = flags,
		.NumDescs = static_cast<UINT>(geometryDescs.size()),
		.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY,
		.pGeometryDescs = geometryDescs.data()
	};
}

inline D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS GetTlasInputs(D3D12_GPU_VIRTUAL_ADDRESS instanceDataAddress,
	uint32_t instanceCount,
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE) 
{
	return
	{
		.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL,
		.Flags = flags,
		.NumDescs = instanceCount,
		.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY,
		.InstanceDescs = instanceDataAddress 
	};

}
