#pragma once

enum class ResourceState //@todo; extend as needed
{
	Common,
	ReadCS,
	ReadPS,
	ReadAny,
	WriteCS,
	WritePS,
	WriteAny,
	RenderTarget,
	DepthRead,
	DepthWrite,
	CopySource,
	CopyDestination,
	ExecuteIndirect,
	RayTracing,
	BuildAccelerationStructure,
	ReadAccelerationStructure, // state for reading from blas while building tlas
	ScratchBuildAccelerationStructure, // used for subsequent builds of acceleration structures reusing the same scratch buffer
	Any
};

template<typename T, typename... Ts> constexpr bool is_one_of = (std::is_same_v<T, Ts> || ...);

//@note: allows user to sepcify empty barrier by not specifying a resource pointer
template<typename T, size_t N> uint32_t RemoveEmptyBarriers(const T(&input)[N], T(&output)[N])
{
	int i = 0;
	for (const auto& element : input)
	{
		if (element.pResource)
		{
			output[i++] = element;
		}
	}
	return i;
}

template <typename... Ts, uint32_t... Sizes>
inline void ResourceTransitions(ID3D12GraphicsCommandList10* commandList, const Ts(&...barriers)[Sizes]) 
{
	constexpr uint32_t barrierGroupCount = sizeof...(Ts);
	D3D12_BARRIER_GROUP barrierGroups[barrierGroupCount];

	int i = 0;
	(
		[&](auto& barriers, uint32_t size)
		{
			using barrier_type = std::remove_cv_t<std::remove_extent_t<std::remove_reference_t<decltype(barriers)>>>;
			static_assert(is_one_of<barrier_type, D3D12_TEXTURE_BARRIER, D3D12_BUFFER_BARRIER, D3D12_GLOBAL_BARRIER>, "Only Barriers allowed in ResourceTransitions.");
			std::remove_cv_t < std::remove_reference_t<decltype(barriers)>> newBarriers;

			if constexpr (std::is_same_v<D3D12_TEXTURE_BARRIER, barrier_type>) 
			{
				barrierGroups[i].NumBarriers = RemoveEmptyBarriers(barriers, newBarriers); 
				barrierGroups[i].Type = D3D12_BARRIER_TYPE_TEXTURE;
				barrierGroups[i].pTextureBarriers = barriers;
			}
			else if constexpr (std::is_same_v<D3D12_BUFFER_BARRIER, barrier_type>)
			{
				barrierGroups[i].NumBarriers = RemoveEmptyBarriers(barriers, newBarriers);
				barrierGroups[i].Type = D3D12_BARRIER_TYPE_BUFFER;
				barrierGroups[i].pBufferBarriers = barriers;
			}
			else if constexpr (std::is_same_v<D3D12_GLOBAL_BARRIER, barrier_type>)
			{
				barrierGroups[i].NumBarriers = size; //no empty barriers for global barriers
				barrierGroups[i].Type = D3D12_BARRIER_TYPE_GLOBAL;
				barrierGroups[i].pGlobalBarriers = barriers;
			}
			
			i++;
		}(barriers, Sizes)
		, ...);

	commandList->Barrier(barrierGroupCount, barrierGroups);
}

D3D12_GLOBAL_BARRIER GlobalBarrier(ResourceState from, ResourceState to);

struct BarrierFields
{
	D3D12_BARRIER_SYNC sync;
	D3D12_BARRIER_ACCESS access;
	D3D12_BARRIER_LAYOUT layout;
};

BarrierFields GetBarrierFieldsFor(ResourceState state);
