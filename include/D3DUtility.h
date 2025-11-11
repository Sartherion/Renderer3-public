#pragma once
#include "DescriptorHeap.h"

void InitializeUtility(ID3D12Device10* device);

// render state helpers
enum class BlendState 
{
    Disabled,
    Additive,
    AlphaBlend,

    Count
};
D3D12_BLEND_DESC GetBlendState(BlendState blendState);

enum class RasterizerState 
{
    NoCull,
    BackFaceCull,
    FrontFaceCull,
    Wireframe,

    Count
};
D3D12_RASTERIZER_DESC GetRasterizerState(RasterizerState rasterizerState);

enum class DepthState 
{
    Disabled,
    Test,
    TestEqual,
    Write,

    Count
};
D3D12_DEPTH_STENCIL_DESC GetDepthState(DepthState depthState);

// pso creation helpers
struct IDxcBlobEncoding;
namespace D3D
{
	extern ComPtr<ID3D12RootSignature> rootSignature;
}

struct GraphicsPsoDesc 
{
	IDxcBlobEncoding* vs = nullptr;
	IDxcBlobEncoding* ps = nullptr;
	IDxcBlobEncoding* ds = nullptr;
	IDxcBlobEncoding* hs = nullptr;
	IDxcBlobEncoding* gs = nullptr;
	D3D12_RASTERIZER_DESC rasterizerState = GetRasterizerState(RasterizerState::BackFaceCull);
	D3D12_BLEND_DESC blendState = GetBlendState(BlendState::Disabled);
	D3D12_DEPTH_STENCIL_DESC depthState = GetDepthState(DepthState::Write);	
	std::span<const DXGI_FORMAT> rtvFormats = {};
	DXGI_FORMAT dsvFormat = DXGI_FORMAT_UNKNOWN;
	D3D12_PRIMITIVE_TOPOLOGY_TYPE primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	uint32_t sampleMask = uint32_t(-1);
	ID3D12RootSignature* rootSignature = D3D::rootSignature.Get();
	D3D12_INPUT_LAYOUT_DESC inputLayout = {};
	DXGI_SAMPLE_DESC sampleDesc = { 1, 0 };
};
ComPtr<ID3D12PipelineState> CreateGraphicsPso(ID3D12Device10* device, const GraphicsPsoDesc&& desc);

struct ComputePsoDesc
{
	IDxcBlobEncoding* cs = nullptr;
	ID3D12RootSignature* rootSignature = D3D::rootSignature.Get();
};
ComPtr<ID3D12PipelineState> CreateComputePso(ID3D12Device10* device, const ComputePsoDesc&& desc);

ComPtr<IDxcBlobEncoding> LoadShaderBinary(LPCWSTR filename);

typedef void (ID3D12GraphicsCommandList10::* SetRootConstantsType)(UINT, UINT, const void*, UINT);

template <uint32_t firstRootConstantOffset = 0, typename...T>
inline void BindGraphicsRootConstants(ID3D12GraphicsCommandList10* commandList, const T&... rootConstants)
{
	SetRootConstants<firstRootConstantOffset>(&ID3D12GraphicsCommandList10::SetGraphicsRoot32BitConstants, commandList, rootConstants...);
}

template <uint32_t firstRootConstantOffset = 0, typename...T>
inline void BindComputeRootConstants(ID3D12GraphicsCommandList10* commandList, const T&... rootConstants)
{
	SetRootConstants<firstRootConstantOffset>(&ID3D12GraphicsCommandList10::SetComputeRoot32BitConstants, commandList, rootConstants...);
}

struct ThreadDimensions
{
	uint32_t dispatchX = 1;
	uint32_t dispatchY = 1;
	uint32_t dispatchZ = 1;
	uint32_t groupX = 8;
	uint32_t groupY = 8;
	uint32_t groupZ = 1;
};

template <typename...T>
concept NotDescriptorHeapAllocation = (!std::same_as<DescriptorHeap::Allocation, T> && ...);

template <uint32_t firstRootConstantOffset = 0, typename...T>
inline void SetRootConstants(SetRootConstantsType function, ID3D12GraphicsCommandList10* commandList, const T&...args) requires NotDescriptorHeapAllocation<T...>
{
	constexpr size_t size = (0 + ... + Max(sizeof(uint32_t), sizeof(T))); //Max because everthing will be padded to 4B

	auto copyArg = [](uint8_t* dstBase, const auto& src, uint32_t& offset)
		{
			uint32_t size = sizeof(src);
			const void* sourcePointer = &src;
			uint32_t extendendSource = 0;
			if (size < sizeof(uint32_t)) //zero pad to 4B for types < 4B, in particular bool
			{
				std::memcpy(&extendendSource, sourcePointer, size);
				sourcePointer = &extendendSource;
				size = sizeof(uint32_t);
			}
			std::memcpy(&dstBase[offset], sourcePointer, size);
			offset += size;
		};

	if constexpr (size > 0)
	{
		uint8_t bytes[size];
		uint32_t offset = 0;
		(..., copyArg(bytes, args, offset));

		assert(offset == size);
		static_assert((size & 3) == 0);
		(commandList->*function)(0, size / 4, bytes, firstRootConstantOffset);
	}
}

template <uint32_t firstRootConstantOffset = 0, typename... T>
inline void DispatchComputePass(ID3D12GraphicsCommandList10* commandList,
	ID3D12PipelineState* pso,
	ThreadDimensions&& threadDimensions,
	LPCWSTR debugName = L"",
	const T&... rootConstants)
{
	const bool emitPixEvent = debugName[0] != L'\0';
	if (emitPixEvent)
	{
		PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, debugName);
	}

	if (pso)
	{
		commandList->SetPipelineState(pso);
	}

	BindComputeRootConstants(commandList, rootConstants...);
	commandList->Dispatch(DivisionRoundUp(threadDimensions.dispatchX, threadDimensions.groupX), 
		DivisionRoundUp(threadDimensions.dispatchY, threadDimensions.groupY),
		DivisionRoundUp(threadDimensions.dispatchZ, threadDimensions.groupZ));

	if (emitPixEvent)
	{
		PIXEndEvent(commandList);
	}
}

template <uint32_t firstRootConstantOffset = 0, typename... T>
inline void DispatchComputePass(ID3D12GraphicsCommandList10* commandList,
	ID3D12PipelineState* pso,
	ThreadDimensions&& threadDimensions,
	const T&... rootConstants)
{
	DispatchComputePass<firstRootConstantOffset>(commandList, pso, std::move(threadDimensions), L"", rootConstants...);
}

template <size_t rootConstantsCount>
inline void DispatchComputePass(ID3D12GraphicsCommandList10* commandList,
	ID3D12PipelineState* pso,
	const uint32_t(&rootConstants)[rootConstantsCount],
	ThreadDimensions&& threadDimensions,
	LPCWSTR debugName = L"")
{
	DispatchComputePass(commandList,
		pso,
		std::move(threadDimensions),
		debugName,
		rootConstants);
}

// clear helpers
struct RWTexture;
void ClearRWTexture(ID3D12GraphicsCommandList10* commandList, const RWTexture& texture, uint32_t(&values)[4]);
void ClearBufferUav(ID3D12GraphicsCommandList10* commandList, DescriptorHeap::Id uavId, uint32_t value, uint32_t elementCount, uint32_t elementsPerThreadCount = 16);
void WriteImmediateValue(ID3D12GraphicsCommandList10* commandList, D3D12_GPU_VIRTUAL_ADDRESS address, uint32_t value);

// miscellaneous
struct TextureProperties;
struct TemporaryRWTexturePool
{
	RWTexture* textures; 
	RWTexture** available;
	uint32_t availableCount = 0;

	void Init(ID3D12Device10* device,
		DescriptorHeap& descriptorHeap,
		const TextureProperties& properties,
		uint32_t elementCount,
		LPCWSTR name = L"");
	void Free();

	RWTexture& Acquire();
	void Release(RWTexture& temporaryTexture);
};

struct TemporaryRWTexture
{
	TemporaryRWTexturePool& pool;
	RWTexture& texture;

	TemporaryRWTexture(TemporaryRWTexturePool& pool) : pool(pool), texture(pool.Acquire()) {}
	~TemporaryRWTexture()
	{
		pool.Release(texture);
	}

	RWTexture& Get();

	operator RWTexture& ();
};

template <typename T>
struct PingPong
{
	T data[2];
	bool index = false;

	operator T&()
	{
		return Current();
	}

	operator const T&() const
	{
		return Current();
	}

	operator T* ()
	{
		return &Current();
	}

	operator const T* () const
	{
		return &Current();
	}

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
		return data[index];
	}

	const T& Current() const
	{
		return data[index];
	}

	T& Other()
	{
		return data[!index];
	}

	const T& Other() const
	{
		return data[!index];
	}

	void Flip()
	{
		index = !index;
	}
};

void DumpToFile(LPCWSTR name, const void* dataPtr, size_t size); 
