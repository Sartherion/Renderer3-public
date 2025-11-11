#include "stdafx.h"
#include "TextureResource.h"

const Range Range::Full = { .begin = 0, .count = Max };

const TextureSubresourceRange TextureSubresourceRange::Full = {
	.mipRange = Range::Full,
	.arrayRange = Range::Full,
	.planeRange = Range::Full,
};

// translates a TextureSubresourceRange into a range that can be used to create a descriptor
static TextureSubresourceRange ClipRange(const TextureResource& parentResource, const TextureSubresourceRange& range);
static bool AllSubresources(const TextureSubresource& subresource);
static D3D12_BARRIER_SUBRESOURCE_RANGE BarrierRange(const TextureSubresource& subresource);

static D3D12_CLEAR_VALUE GetDefaultClearValue(DXGI_FORMAT format, TextureType type);
static D3D12_BARRIER_LAYOUT GetDefaultLayout(TextureType type, TextureUsage usage);


D3D12_TEXTURE_BARRIER BarrierCSWriteToRead(const TextureSubresource& subresource)
{
	return subresource.Barrier(ResourceState::WriteCS, ResourceState::ReadCS);
}

D3D12_TEXTURE_BARRIER BarrierCSReadToWrite(const TextureSubresource& subresource)
{
	return subresource.Barrier(ResourceState::ReadCS, ResourceState::WriteCS);
}

D3D12_TEXTURE_BARRIER BarrierCSWriteToWrite(const TextureSubresource& subresource)
{
	return subresource.Barrier(ResourceState::WriteCS, ResourceState::WriteCS);
}

TextureResource CreateTextureResource(ID3D12Device10* device,
	const TextureProperties& properties,
	TextureType type,
	TextureUsage usage,
	LPCWSTR name,
	TextureMemoryType memoryType,
	D3D12_BARRIER_LAYOUT initialLayout,
	const D3D12_CLEAR_VALUE* clearValue)
{
	const D3D12_RESOURCE_DIMENSION resourceDimension = GetResourceDimension(properties);
	const bool isArray = IsArray(properties);
	const bool isReadWrite = usage == TextureUsage::ReadWrite;
	const bool isRenderTarget = type == TextureType::RenderTarget;
	const bool isDepthStencil = type == TextureType::DepthBuffer;
	const bool isShaderResource = usage != TextureUsage::DisallowShader;
	const bool needsClearValue = isRenderTarget || isDepthStencil;

	if (initialLayout == D3D12_BARRIER_LAYOUT_UNDEFINED)
	{
		initialLayout = GetDefaultLayout(type, usage);
	}

	D3D12_CLEAR_VALUE defaultClearValue = GetDefaultClearValue(properties.format, type);
	if (needsClearValue && !clearValue)
	{
		clearValue = &defaultClearValue;
	}
	assert(!(resourceDimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D && isArray));
	assert(!(resourceDimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D && isDepthStencil));
	assert(isRenderTarget || isDepthStencil || isShaderResource);

	D3D12_RESOURCE_DESC1 resourceDesc = {};
	resourceDesc.Dimension = resourceDimension;
	resourceDesc.Format = properties.format;
	resourceDesc.MipLevels = properties.mipCount;
	resourceDesc.Alignment = 0;
	resourceDesc.DepthOrArraySize = isArray ? properties.arraySize : properties.depth;
	resourceDesc.Flags = (isReadWrite ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE)
		| (isRenderTarget ? D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET : D3D12_RESOURCE_FLAG_NONE)
		| (isDepthStencil ? D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL : D3D12_RESOURCE_FLAG_NONE)
		| (!isShaderResource ? D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE : D3D12_RESOURCE_FLAG_NONE); //note: apparently render targets must not set the shader resource flag, otherwise the debug layer reports it?
	resourceDesc.Width = properties.width;
	resourceDesc.Height = properties.height;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resourceDesc.SampleDesc.Count = properties.sampleCount;
	resourceDesc.SampleDesc.Quality = 0;

	D3D12_HEAP_PROPERTIES heapProperties;
	heapProperties.Type = memoryType == TextureMemoryType::Default ? D3D12_HEAP_TYPE_DEFAULT : D3D12_HEAP_TYPE_GPU_UPLOAD;
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProperties.CreationNodeMask = 0;
	heapProperties.VisibleNodeMask = 0;
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	ComPtr<ID3D12Resource1> resource;
	CheckForErrors(device->CreateCommittedResource3(&heapProperties,
		D3D12_HEAP_FLAG_CREATE_NOT_ZEROED,
		&resourceDesc,
		initialLayout,
		clearValue,
		nullptr,
		0,
		nullptr,
		IID_PPV_ARGS(&resource)));
	resource->SetName(name);

	return { .ptr = resource, .properties = properties, .type = type, .usage = usage };
}

bool IsArray(const TextureProperties& textureProperties)
{
	return textureProperties.arraySize > 1;
}

D3D12_RESOURCE_DIMENSION GetResourceDimension(const TextureProperties& textureProperties)
{
	if (textureProperties.depth > 1)
	{
		return D3D12_RESOURCE_DIMENSION_TEXTURE3D;
	}

	if (textureProperties.width > 1)
	{
		return D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	}

	return D3D12_RESOURCE_DIMENSION_TEXTURE1D;
}

D3D12_CLEAR_VALUE GetDefaultClearValue(DXGI_FORMAT format, TextureType type)
{
	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Format = format;

	if (type == TextureType::DepthBuffer)
	{
		clearValue.DepthStencil = { 1.0f, 0 }; 
	}

	return clearValue;
}

D3D12_BARRIER_LAYOUT GetDefaultLayout(TextureType type, TextureUsage usage)
{
	switch (type)
	{
	case TextureType::ShaderResource:
		assert(usage != TextureUsage::DisallowShader);
		return usage == TextureUsage::ReadWrite ? D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_UNORDERED_ACCESS : D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_SHADER_RESOURCE;
	case TextureType::RenderTarget:
		return D3D12_BARRIER_LAYOUT_RENDER_TARGET;
	case TextureType::DepthBuffer:
		return D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE;
	default:
		assert(false);
		return{};
	}
}

TextureSubresource TextureResource::GetSubresource(const TextureSubresourceRange range) const
{
	return
	{
		.parentResourcePtr = this,
		.range = ClipRange(*this, range)
	};
}

TextureSubresourceRange ClipRange(const TextureResource& parentResource, const TextureSubresourceRange& range)
{
	TextureSubresourceRange result = range;

	if (result.mipRange.count == Range::Max)
	{
		result.mipRange.count = parentResource.properties.mipCount - result.mipRange.begin;
	}

	if (result.arrayRange.count == Range::Max)
	{
		result.arrayRange.count = parentResource.properties.arraySize - result.arrayRange.begin;
	}

	if (result.planeRange.count == Range::Max)
	{
		result.planeRange.count = 1; //@todo: plane support
	}

	return result;
}

bool AllSubresources(const TextureSubresource& subresource)
{
	const TextureSubresourceRange& range = subresource.range;
	const TextureProperties& properties = subresource.parentResourcePtr->properties;
	return range.mipRange.begin == 0 && range.mipRange.count == properties.mipCount
		&& range.arrayRange.begin == 0 && range.arrayRange.count == properties.arraySize
		&& range.planeRange.begin == 0 && range.planeRange.count == 1; //@todo: plane support
}

D3D12_BARRIER_SUBRESOURCE_RANGE BarrierRange(const TextureSubresource& subresource) 
{
	if (AllSubresources(subresource))
	{
		return barrierSubresourceRangeFull;
	}

	const TextureSubresourceRange& range = subresource.range;

	return
	{
		.IndexOrFirstMipLevel = range.mipRange.begin,
		.NumMipLevels = range.mipRange.count,
		.FirstArraySlice = range.arrayRange.begin,
		.NumArraySlices = range.arrayRange.count,
		.FirstPlane = range.planeRange.begin,
		.NumPlanes = range.planeRange.count
	};
}

D3D12_TEXTURE_BARRIER TextureSubresource::Barrier(ResourceState from, ResourceState to, bool discard) const
{
	return parentResourcePtr->Barrier(from, to, BarrierRange(*this), discard);
}

D3D12_TEXTURE_BARRIER TextureSubresource::Done(ResourceState from, D3D12_BARRIER_LAYOUT layout, bool discard) const
{
	return parentResourcePtr->Done(from, layout, BarrierRange(*this), discard);
}

