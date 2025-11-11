#include "stdafx.h"
#include "D3DBarrierHelpers.h"

D3D12_GLOBAL_BARRIER GlobalBarrier(ResourceState from, ResourceState to)
{
	BarrierFields fromFields = GetBarrierFieldsFor(from);
	BarrierFields toFields = GetBarrierFieldsFor(to);

	return
	{
	.SyncBefore = fromFields.sync,
	.SyncAfter = toFields.sync,
	.AccessBefore = fromFields.access,
	.AccessAfter = toFields.access
	};
}

BarrierFields GetBarrierFieldsFor(ResourceState state)
{
	BarrierFields barrierFields{};

	using enum ResourceState;
	switch (state)
	{
	case Common:
		barrierFields.access = D3D12_BARRIER_ACCESS_NO_ACCESS;
		barrierFields.sync = D3D12_BARRIER_SYNC_NONE;
		barrierFields.layout = D3D12_BARRIER_LAYOUT_COMMON;
		break;
	case ReadCS:
		barrierFields.access = D3D12_BARRIER_ACCESS_SHADER_RESOURCE;
		barrierFields.sync = D3D12_BARRIER_SYNC_COMPUTE_SHADING;
		barrierFields.layout = D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_SHADER_RESOURCE;
		break;
	case ReadPS:
		barrierFields.access = D3D12_BARRIER_ACCESS_SHADER_RESOURCE;
		barrierFields.sync = D3D12_BARRIER_SYNC_PIXEL_SHADING;
		barrierFields.layout = D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_SHADER_RESOURCE;
		break;
	case ReadAny:
		barrierFields.access = D3D12_BARRIER_ACCESS_SHADER_RESOURCE;
		barrierFields.sync = D3D12_BARRIER_SYNC_ALL_SHADING;
		barrierFields.layout = D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_SHADER_RESOURCE;
		break;
	case WriteCS:
		barrierFields.access = D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
		barrierFields.sync = D3D12_BARRIER_SYNC_COMPUTE_SHADING;
		barrierFields.layout = D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_UNORDERED_ACCESS;
		break;
	case WritePS:
		barrierFields.access = D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
		barrierFields.sync = D3D12_BARRIER_SYNC_PIXEL_SHADING;
		barrierFields.layout = D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_UNORDERED_ACCESS;
		break;
	case WriteAny:
		barrierFields.access = D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
		barrierFields.sync = D3D12_BARRIER_SYNC_ALL_SHADING;
		barrierFields.layout = D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_UNORDERED_ACCESS;
		break;
	case RenderTarget:
		barrierFields.access = D3D12_BARRIER_ACCESS_RENDER_TARGET;
		barrierFields.sync = D3D12_BARRIER_SYNC_RENDER_TARGET;
		barrierFields.layout = D3D12_BARRIER_LAYOUT_RENDER_TARGET;
		break;
	case DepthRead:
		barrierFields.access = D3D12_BARRIER_ACCESS_DEPTH_STENCIL_READ;
		barrierFields.sync = D3D12_BARRIER_SYNC_DEPTH_STENCIL;
		barrierFields.layout = D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_READ;
		break;
	case DepthWrite:
		barrierFields.access = D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE;
		barrierFields.sync = D3D12_BARRIER_SYNC_DEPTH_STENCIL;
		barrierFields.layout = D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE;
		break;
	case CopySource:
		barrierFields.access = D3D12_BARRIER_ACCESS_COPY_SOURCE;
		barrierFields.sync = D3D12_BARRIER_SYNC_COPY;
		barrierFields.layout = D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_COPY_SOURCE;
		break;
	case CopyDestination:
		barrierFields.access = D3D12_BARRIER_ACCESS_COPY_DEST;
		barrierFields.sync = D3D12_BARRIER_SYNC_COPY;
		barrierFields.layout = D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_COPY_DEST;
		break;
	case RayTracing:
		barrierFields.access = D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_READ;
		barrierFields.sync = D3D12_BARRIER_SYNC_RAYTRACING;
		barrierFields.layout = D3D12_BARRIER_LAYOUT_UNDEFINED;
		break;
	case BuildAccelerationStructure:
		barrierFields.access = D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_WRITE;
		barrierFields.sync = D3D12_BARRIER_SYNC_BUILD_RAYTRACING_ACCELERATION_STRUCTURE;
		barrierFields.layout = D3D12_BARRIER_LAYOUT_UNDEFINED;
		break;
	case ReadAccelerationStructure:
		barrierFields.access = D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_READ;
		barrierFields.sync = D3D12_BARRIER_SYNC_BUILD_RAYTRACING_ACCELERATION_STRUCTURE;
		barrierFields.layout = D3D12_BARRIER_LAYOUT_UNDEFINED;
		break;
	case ScratchBuildAccelerationStructure:
		barrierFields.access = D3D12_BARRIER_ACCESS_UNORDERED_ACCESS; //@note: scratch buffer access is treated as UAV
		barrierFields.sync = D3D12_BARRIER_SYNC_BUILD_RAYTRACING_ACCELERATION_STRUCTURE;
		barrierFields.layout = D3D12_BARRIER_LAYOUT_UNDEFINED;
		break;
	case Any:
		barrierFields.access = D3D12_BARRIER_ACCESS_COMMON;
		barrierFields.sync = D3D12_BARRIER_SYNC_ALL;
		barrierFields.layout = D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_COMMON;
		break;
	}

	return barrierFields;
}
