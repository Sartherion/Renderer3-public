#pragma once
#include "BufferMemory.h"
#include "D3DUtility.h"
#include "DescriptorHeap.h"
#include "TextureResource.h"

//read-only texture
struct Texture : TextureResource
{
	DescriptorHeap::Allocation srvId;
};

//texture that can also be bound as UAV
struct RWTexture : Texture
{
	DescriptorHeap::Allocation uavId;
};

enum class TextureArrayType
{
	Unspecified,
	CubeMap,
	CubeMapArray
};

Texture CreateTexture(ID3D12Device10* device,
	const TextureProperties& properties,
	DescriptorHeap& srvHeap,
	LPCWSTR name = L"",
	TextureMemoryType memoryType = TextureMemoryType::CPUVisible, //read-only textures will likely be initialized from CPU. @note: this requires GPU-Upload heap support
	D3D12_BARRIER_LAYOUT initialLayout = D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_SHADER_RESOURCE,
	TextureArrayType arrayType = TextureArrayType::Unspecified);

RWTexture CreateRWTexture(ID3D12Device10* device,
	const TextureProperties& properties,
	DescriptorHeap& srvHeap,
	LPCWSTR name = L"",
	TextureMemoryType memoryType = TextureMemoryType::Default,
	D3D12_BARRIER_LAYOUT initialLayout = D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_UNORDERED_ACCESS,
	TextureArrayType arrayType = TextureArrayType::Unspecified);

void DestroySafe(Texture& texture);
void DestroySafe(RWTexture& texture);

D3D12_SHADER_RESOURCE_VIEW_DESC GetSubresourceSrvDesc(const TextureSubresource& subresource, TextureArrayType type = TextureArrayType::Unspecified);
D3D12_UNORDERED_ACCESS_VIEW_DESC GetSubresourceUavDesc(const TextureSubresource& subresource, TextureArrayType type = TextureArrayType::Unspecified);

DescriptorHeap::Allocation CreateSrvOnHeap(DescriptorHeap& descriptorHeap, const TextureSubresource& subresource, TextureArrayType type = TextureArrayType::Unspecified);
DescriptorHeap::Allocation CreateUavOnHeap(DescriptorHeap& descriptorHeap, const TextureSubresource& subresource, TextureArrayType type = TextureArrayType::Unspecified);

DescriptorHeap::Id CreateSrvOnHeap(TemporaryDescriptorHeap& descriptorHeap, const TextureSubresource& subresource, TextureArrayType type = TextureArrayType::Unspecified);
DescriptorHeap::Id CreateUavOnHeap(TemporaryDescriptorHeap& descriptorHeap, const TextureSubresource& subresource, TextureArrayType type = TextureArrayType::Unspecified);

enum class ColorMode
{
	NotSpecified,
	ForceSRGB,
	ForceLinear
};

Texture LoadTexture(LPCWSTR filename, ID3D12Device10* device, DescriptorHeap& srvHeap, ColorMode colorMode = ColorMode::NotSpecified, bool bNoMip = false);

//Translates the depth buffer format to a suitable regular texture format.
constexpr DXGI_FORMAT TranslateDepthBufferFormat(DXGI_FORMAT depthBufferFormat)
{
	DXGI_FORMAT srvFormat = DXGI_FORMAT_UNKNOWN;
	switch (depthBufferFormat)
	{
	case DXGI_FORMAT_D32_FLOAT:
		srvFormat = DXGI_FORMAT_R32_FLOAT;
		break;
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
        srvFormat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        break;
    case DXGI_FORMAT_D16_UNORM:
        srvFormat = DXGI_FORMAT_R16_UNORM;
        break;
	}

	return srvFormat;
}

struct Texture2DDimensions
{
	uint32_t width;
	uint32_t height;
	float texelWidth;
	float texelHeight;
};

inline Texture2DDimensions GetTexture2DDimensions(const Texture& texture)
{
	return
	{
		.width = texture.properties.width,
		.height = texture.properties.height,
		.texelWidth = 1.0f / texture.properties.width,
		.texelHeight = 1.0f / texture.properties.height 
	};
}

// Creates current buffer in unordered access and other buffer in shader resource view state
PingPong<RWTexture> CreatePingPongRWTextures(ID3D12Device10* device,
	const TextureProperties& properties,
	DescriptorHeap& descriptorHeap,
	LPCWSTR name);