#include "stdafx.h"
#include "Texture.h"

#include "D3DUtility.h"
#include "Frame.h"


D3D12_SRV_DIMENSION GetSrvDimension(const TextureProperties& textureProperties, TextureArrayType arrayType);
D3D12_UAV_DIMENSION GetUavDimension(const TextureProperties& textureProperties); 

Texture LoadTexture(LPCWSTR filename, ID3D12Device10* device, DescriptorHeap& descriptorHeap, ColorMode colorMode, bool bNoMip)
{
	Texture result;
	DirectX::ScratchImage image;

	std::filesystem::path path(filename);
	auto extension = path.extension();
	if (extension.compare(L".dds") == 0 || extension.compare(L".DDS") == 0)
	{
		CheckForErrors(DirectX::LoadFromDDSFile(filename, DirectX::DDS_FLAGS_NONE, nullptr, image));
	}
	else if (extension.compare(L".tga") == 0 || extension.compare(L".TGA") == 0)
	{
		if (bNoMip)
		{
			CheckForErrors(DirectX::LoadFromTGAFile(filename, nullptr, image));
		}
		else
		{
			DirectX::ScratchImage tempImage;
			CheckForErrors(DirectX::LoadFromTGAFile(filename, nullptr, tempImage));
			CheckForErrors(DirectX::GenerateMipMaps(*tempImage.GetImage(0, 0, 0), DirectX::TEX_FILTER_DEFAULT, 0, image, false));
		}
	}
	else
	{
		if (bNoMip)
		{
			CheckForErrors(DirectX::LoadFromWICFile(filename, DirectX::WIC_FLAGS_NONE, nullptr, image));
		}
		else
		{
			DirectX::ScratchImage tempImage;
			CheckForErrors(DirectX::LoadFromWICFile(filename, DirectX::WIC_FLAGS_NONE, nullptr, tempImage));
			CheckForErrors(DirectX::GenerateMipMaps(*tempImage.GetImage(0, 0, 0), DirectX::TEX_FILTER_DEFAULT, 0, image, false));
		}
	}

	const DirectX::TexMetadata& metadata = image.GetMetadata();
	DXGI_FORMAT format = metadata.format;
	switch (colorMode)
	{
	case ColorMode::ForceSRGB:
		format = DirectX::MakeSRGB(format);
		break;

	case ColorMode::ForceLinear:
		format = DirectX::MakeLinear(format);
		break;
	default:
		break;
	}


	result = CreateTexture(device,
		{
			.format = format,
			.width = (uint32_t)metadata.width,
			.height = (uint32_t)metadata.height,
			.depth = (uint32_t)metadata.depth,
			.arraySize = (uint32_t)metadata.arraySize,
			.mipCount = (uint32_t)metadata.mipLevels
		},
		descriptorHeap,
		filename,
		TextureMemoryType::CPUVisible,
		D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_SHADER_RESOURCE,
		metadata.IsCubemap() ? TextureArrayType::CubeMap : TextureArrayType::Unspecified);

	TextureProperties& properties = result.properties;
	for (unsigned int iArray = 0; iArray < properties.arraySize; iArray++)
	{
		for (unsigned int iMip = 0; iMip < properties.mipCount; iMip++)
		{
			const unsigned int iSubresource = iMip + iArray * properties.mipCount;

			const DirectX::Image* subImage = image.GetImage(iMip, iArray, 0);
			const uint8_t* srcImage = subImage->pixels;

			result.ptr->WriteToSubresource(iSubresource, nullptr, srcImage, (uint32_t)subImage->rowPitch, (uint32_t)subImage->slicePitch);
		}
	}
	return result;
}

Texture CreateTexture(ID3D12Device10* device, 
	const TextureProperties& properties,
	DescriptorHeap& descriptorHeap, 
	LPCWSTR name,
	TextureMemoryType memoryType,
	D3D12_BARRIER_LAYOUT initialLayout,
	TextureArrayType arrayType)
{
	Texture result;
	static_cast<TextureResource&>(result) = CreateTextureResource(device,
		properties,
		TextureType::ShaderResource,
		TextureUsage::Default,
		name,
		memoryType,
		initialLayout);

	
	result.srvId = CreateSrvOnHeap(descriptorHeap, result, arrayType);

	return result;
}

RWTexture CreateRWTexture(ID3D12Device10* device,
	const TextureProperties& properties,
	DescriptorHeap& descriptorHeap,
	LPCWSTR name,
	TextureMemoryType memoryType,
	D3D12_BARRIER_LAYOUT initialLayout,
	TextureArrayType arrayType)
{
	RWTexture result;
	static_cast<TextureResource&>(result) = CreateTextureResource(device,
		properties,
		TextureType::ShaderResource,
		TextureUsage::ReadWrite,
		name,
		memoryType,
		initialLayout);

	result.srvId = CreateSrvOnHeap(descriptorHeap, result, arrayType);
	result.uavId = CreateUavOnHeap(descriptorHeap, result.ptr.Get());

	return result;
}

void DestroySafe(Texture& texture)
{
	Frame::SafeRelease(texture.srvId);
	Frame::SafeRelease(std::move(texture.ptr));
	texture.ptr = nullptr;
}

void DestroySafe(RWTexture& texture)
{
	Frame::SafeRelease(texture.uavId);
	DestroySafe(static_cast<Texture&>(texture));
}

PingPong<RWTexture> CreatePingPongRWTextures(ID3D12Device10* device, const TextureProperties& properties, DescriptorHeap& descriptorHeap, LPCWSTR name)
{
	const size_t nameLength = wcslen(name);
	assert(nameLength < 64 - 2);
	WCHAR extendedName[64];
	wcscpy(extendedName, name);
	wcscat(extendedName, L"0");

	PingPong<RWTexture> result;

	result.Current() = CreateRWTexture(device,
		properties,
		descriptorHeap,
		extendedName);

	extendedName[nameLength] = L'1';
	result.Other() = CreateRWTexture(device,
		properties,
		descriptorHeap,
		extendedName,
		TextureMemoryType::Default,
		D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_SHADER_RESOURCE);

	return result;
}

D3D12_TEXTURE_BARRIER TextureResource::Barrier(ResourceState from, ResourceState to, const D3D12_BARRIER_SUBRESOURCE_RANGE& subresourceRange, bool discard) const
{
	D3D12_TEXTURE_BARRIER textureBarrier;
	textureBarrier.pResource = ptr.Get();
	
	textureBarrier.Subresources = subresourceRange;

	textureBarrier.Flags = discard ? D3D12_TEXTURE_BARRIER_FLAG_DISCARD : D3D12_TEXTURE_BARRIER_FLAG_NONE;

	BarrierFields fromFields = GetBarrierFieldsFor(from);
	textureBarrier.AccessBefore = fromFields.access;
	textureBarrier.SyncBefore = fromFields.sync;
	textureBarrier.LayoutBefore = fromFields.layout;

	BarrierFields toFields = GetBarrierFieldsFor(to);
	textureBarrier.AccessAfter = toFields.access;
	textureBarrier.SyncAfter = toFields.sync;
	textureBarrier.LayoutAfter = toFields.layout;

	return textureBarrier;
}

D3D12_TEXTURE_BARRIER TextureResource::Done(ResourceState from,
	D3D12_BARRIER_LAYOUT layout,
	const D3D12_BARRIER_SUBRESOURCE_RANGE& subresourceRange,
	bool discard) const
{
	D3D12_TEXTURE_BARRIER textureBarrier;
	textureBarrier.pResource = ptr.Get();
	textureBarrier.Subresources = subresourceRange;

	textureBarrier.Flags = discard ? D3D12_TEXTURE_BARRIER_FLAG_DISCARD : D3D12_TEXTURE_BARRIER_FLAG_NONE;

	BarrierFields fromFields = GetBarrierFieldsFor(from);
	textureBarrier.AccessBefore = fromFields.access;
	textureBarrier.SyncBefore = fromFields.sync;
	textureBarrier.LayoutBefore = fromFields.layout;

	textureBarrier.AccessAfter = D3D12_BARRIER_ACCESS_NO_ACCESS;
	textureBarrier.SyncAfter = D3D12_BARRIER_SYNC_NONE;
	textureBarrier.LayoutAfter = layout;
	
	return textureBarrier;
}

D3D12_TEXTURE_BARRIER TextureResource::SplitBarrierBegin(ResourceState from,
	ResourceState to,
	SplitBarrierState& splitBarrierState,
	const D3D12_BARRIER_SUBRESOURCE_RANGE& subresourceRange,
	bool discard) const
{
	D3D12_TEXTURE_BARRIER firstBarrier = Barrier(from, to, subresourceRange, discard);
	splitBarrierState = firstBarrier;
	firstBarrier.SyncAfter = D3D12_BARRIER_SYNC_SPLIT;
	splitBarrierState.SyncBefore = D3D12_BARRIER_SYNC_SPLIT;
	
	return firstBarrier;
}

D3D12_TEXTURE_BARRIER TextureResource::SplitBarrierEnd(const SplitBarrierState& splitBarrierState) const
{
	return splitBarrierState;
}

D3D12_SRV_DIMENSION GetSrvDimension(const TextureProperties& textureProperties, TextureArrayType arrayType)
{
	bool isMultiSample = textureProperties.sampleCount > 1; 
	bool isArray = IsArray(textureProperties);
	D3D12_RESOURCE_DIMENSION dimension = GetResourceDimension(textureProperties);

	if (arrayType != TextureArrayType::Unspecified)
	{
		assert(dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D);
		assert(isArray);
		return arrayType == TextureArrayType::CubeMapArray ? D3D12_SRV_DIMENSION_TEXTURECUBEARRAY : D3D12_SRV_DIMENSION_TEXTURECUBE;
	}

	switch (dimension)
	{
	case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
		return isArray ? D3D12_SRV_DIMENSION_TEXTURE1DARRAY : D3D12_SRV_DIMENSION_TEXTURE1D;
	case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
		return isArray ? D3D12_SRV_DIMENSION_TEXTURE2DARRAY : D3D12_SRV_DIMENSION_TEXTURE2D;
	case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
		assert(!isArray);
		return D3D12_SRV_DIMENSION_TEXTURE3D;
	default:
		assert(false);
		return {};
	}
}

D3D12_UAV_DIMENSION GetUavDimension(const TextureProperties& textureProperties)
{
	bool isMultiSample = textureProperties.sampleCount > 1;
	bool isArray = IsArray(textureProperties);
	D3D12_RESOURCE_DIMENSION dimension = GetResourceDimension(textureProperties);

	switch (dimension)
	{
	case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
		return isArray ? D3D12_UAV_DIMENSION_TEXTURE1DARRAY : D3D12_UAV_DIMENSION_TEXTURE1D;
	case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
		return isArray ? D3D12_UAV_DIMENSION_TEXTURE2DARRAY : D3D12_UAV_DIMENSION_TEXTURE2D;
	case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
		assert(!isArray);
		return D3D12_UAV_DIMENSION_TEXTURE3D;
	default:
		assert(false);
		return{};
	}
}

D3D12_SHADER_RESOURCE_VIEW_DESC GetSubresourceSrvDesc(const TextureSubresource& subresource, TextureArrayType type)
{
	D3D12_SRV_DIMENSION viewDimension = GetSrvDimension(subresource.parentResourcePtr->properties, type);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{
		.Format = subresource.parentResourcePtr->type == TextureType::DepthBuffer ?
			TranslateDepthBufferFormat(subresource.parentResourcePtr->properties.format) :
			subresource.parentResourcePtr->properties.format,
		.ViewDimension = viewDimension,
		.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING
	};

	const TextureSubresourceRange& range = subresource.range;
	assert(range.planeRange.count == 1);

	switch (viewDimension)
	{
	case D3D12_SRV_DIMENSION_TEXTURE2D:
		srvDesc.Texture2D = {
			.MostDetailedMip = range.mipRange.begin,
			.MipLevels = range.mipRange.count,
			.PlaneSlice = range.planeRange.begin,
			.ResourceMinLODClamp = 0.0f
		};
		break;
	case D3D12_SRV_DIMENSION_TEXTURE2DARRAY:
		srvDesc.Texture2DArray = {
			.MostDetailedMip = range.mipRange.begin,
			.MipLevels = range.mipRange.count,
			.FirstArraySlice = range.arrayRange.begin,
			.ArraySize = range.arrayRange.count,
			.PlaneSlice = range.planeRange.begin,
			.ResourceMinLODClamp = 0.0f
		};
		break;
	case D3D12_SRV_DIMENSION_TEXTURECUBE:
		srvDesc.TextureCube = {
			.MostDetailedMip = range.mipRange.begin,
			.MipLevels = range.mipRange.count,
			.ResourceMinLODClamp = 0.0f
		};
		break;
	case D3D12_SRV_DIMENSION_TEXTURECUBEARRAY:
		srvDesc.TextureCubeArray = {
			.MostDetailedMip = range.mipRange.begin,
			.MipLevels = range.mipRange.count,
			.First2DArrayFace = range.arrayRange.begin,
			.NumCubes = range.arrayRange.count / 6,
			.ResourceMinLODClamp = 0.0f
		};
		break;
	default:
		assert(false);
		break;
	}
	return srvDesc;
}

D3D12_UNORDERED_ACCESS_VIEW_DESC GetSubresourceUavDesc(const TextureSubresource& subresource, TextureArrayType type)
{
	D3D12_UAV_DIMENSION viewDimension = GetUavDimension(subresource.parentResourcePtr->properties);

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{
		.Format = subresource.parentResourcePtr->type == TextureType::DepthBuffer ?
			TranslateDepthBufferFormat(subresource.parentResourcePtr->properties.format) :
			subresource.parentResourcePtr->properties.format,
		.ViewDimension = viewDimension
	};

	const TextureSubresourceRange& range = subresource.range;
	assert(range.planeRange.count == 1 && range.mipRange.count == 1);

	switch (viewDimension)
	{
	case D3D12_UAV_DIMENSION_TEXTURE2D:
		uavDesc.Texture2D = {
			.MipSlice = range.mipRange.begin,
			.PlaneSlice = range.planeRange.begin
		};
		break;
	case D3D12_UAV_DIMENSION_TEXTURE2DARRAY:
		uavDesc.Texture2DArray = {
			.MipSlice = range.mipRange.begin,
			.FirstArraySlice = range.arrayRange.begin,
			.ArraySize = range.arrayRange.count,
			.PlaneSlice = range.planeRange.begin
		};
		break;
	default:
		assert(false);
		break;
	}
	return uavDesc;
}

DescriptorHeap::Allocation CreateSrvOnHeap(DescriptorHeap& descriptorHeap, const TextureSubresource& subresource, TextureArrayType type)
{
	return CreateSrvOnHeap(descriptorHeap, subresource.parentResourcePtr->ptr.Get(), GetSubresourceSrvDesc(subresource, type));
}

DescriptorHeap::Allocation CreateUavOnHeap(DescriptorHeap& descriptorHeap, const TextureSubresource& subresource, TextureArrayType type)
{
	return CreateUavOnHeap(descriptorHeap, subresource.parentResourcePtr->ptr.Get(), GetSubresourceUavDesc(subresource, type));
}

DescriptorHeap::Id CreateSrvOnHeap(TemporaryDescriptorHeap& descriptorHeap, const TextureSubresource& subresource, TextureArrayType type)
{
	return CreateSrvOnHeap(descriptorHeap, subresource.parentResourcePtr->ptr.Get(), GetSubresourceSrvDesc(subresource, type));
}

DescriptorHeap::Id CreateUavOnHeap(TemporaryDescriptorHeap& descriptorHeap, const TextureSubresource& subresource, TextureArrayType type)
{
	return CreateUavOnHeap(descriptorHeap, subresource.parentResourcePtr->ptr.Get(), GetSubresourceUavDesc(subresource, type));
}
