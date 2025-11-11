#pragma once
#include "D3DBarrierHelpers.h"

enum class TextureType : uint8_t
{
	ShaderResource,
	RenderTarget,
	DepthBuffer
};

enum class TextureUsage : uint8_t
{
	Default,
	ReadWrite,
	DisallowShader
};

enum class TextureMemoryType
{
	Default,
	CPUVisible
};

struct TextureProperties
{
	DXGI_FORMAT format;
	uint32_t width;
	uint32_t height = 1;
	uint32_t depth = 1;
	uint32_t arraySize = 1;
	uint32_t mipCount = 1;
	uint32_t sampleCount = 1;
};

using  SplitBarrierState = D3D12_TEXTURE_BARRIER;

struct Range
{
	uint32_t begin = 0;
	uint32_t count = 1;

	static const Range Full;
	static constexpr uint32_t Max = uint32_t(-1);

	bool IsRangeFull() const
	{
		return *this == Full;
	}

	uint32_t End() const
	{
		return begin + count;
	}

	bool operator==(const Range& other) const = default;
};

struct TextureSubresourceRange
{
	Range mipRange = {};
	Range arrayRange = {};
	Range planeRange = {};

	static const TextureSubresourceRange Full;

	bool operator==(const TextureSubresourceRange& other) const = default;
};

struct TextureResource;
struct TextureSubresource
{
	const TextureResource* parentResourcePtr;
	TextureSubresourceRange range = TextureSubresourceRange::Full;

	[[nodiscard]]
	D3D12_TEXTURE_BARRIER Barrier(ResourceState from, ResourceState to, bool discard = false) const;

	[[nodiscard]]
	D3D12_TEXTURE_BARRIER Done(ResourceState from, D3D12_BARRIER_LAYOUT layout, bool discard = false) const;
};

constexpr D3D12_BARRIER_SUBRESOURCE_RANGE barrierSubresourceRangeFull = { .IndexOrFirstMipLevel = uint32_t(-1), .NumMipLevels = 0 }; //@note: for barrier subresources, these values for  indicate all subresources, but this is not true for views

struct TextureResource
{
	ComPtr<ID3D12Resource1> ptr;
	TextureProperties properties;
	TextureType type;
	TextureUsage usage;

	operator TextureSubresource() const
	{
		return GetSubresource();
	}

	TextureSubresource GetSubresource(const TextureSubresourceRange range = TextureSubresourceRange::Full) const;

	[[nodiscard]]
	D3D12_TEXTURE_BARRIER Barrier(ResourceState from,
		ResourceState to,
		const D3D12_BARRIER_SUBRESOURCE_RANGE& subresourceRange = barrierSubresourceRangeFull,
		bool discard = false) const;

	[[nodiscard]]
	D3D12_TEXTURE_BARRIER Done(ResourceState from,
		D3D12_BARRIER_LAYOUT layout,
		const D3D12_BARRIER_SUBRESOURCE_RANGE& subresourceRange = barrierSubresourceRangeFull,
		bool discard = false) const;

	//@todo: untested
	[[nodiscard]]
	D3D12_TEXTURE_BARRIER SplitBarrierBegin(ResourceState from,
		ResourceState to,
		SplitBarrierState& splitBarrierState,
		const D3D12_BARRIER_SUBRESOURCE_RANGE& subresourceRange = barrierSubresourceRangeFull,
		bool discard = false) const;
	
	[[nodiscard]]
	D3D12_TEXTURE_BARRIER SplitBarrierEnd(const SplitBarrierState& splitBarrierState) const;

	bool HasSameDimensionsAs(const TextureResource& other) const
	{
		return properties.width == other.properties.width
			&& properties.height == other.properties.height
			&& properties.depth == other.properties.depth;
	}
};

D3D12_TEXTURE_BARRIER BarrierCSWriteToRead(const TextureSubresource& subresource);
D3D12_TEXTURE_BARRIER BarrierCSReadToWrite(const TextureSubresource& subresource);
D3D12_TEXTURE_BARRIER BarrierCSWriteToWrite(const TextureSubresource& subresource);

TextureResource CreateTextureResource(ID3D12Device10* device,
	const TextureProperties& properties,
	TextureType type = TextureType::ShaderResource,
	TextureUsage usage = TextureUsage::Default,
	LPCWSTR name = L"",
	TextureMemoryType memoryType = TextureMemoryType::Default,
	D3D12_BARRIER_LAYOUT initialLayout = D3D12_BARRIER_LAYOUT_UNDEFINED,
	const D3D12_CLEAR_VALUE* clearValue = nullptr);

//TextureProperties helper
D3D12_RESOURCE_DIMENSION GetResourceDimension(const TextureProperties& textureProperties);
bool IsArray(const TextureProperties& textureProperties);


