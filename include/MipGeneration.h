#pragma once
#include "TextureResource.h"
#include "DescriptorHeap.h"

struct RWTexture;
struct TemporaryDescriptorHeap;

namespace MipGenerator
{
	enum class SampleMode
	{
		Bilinear,
		Minimum,
		Maximum,
		MinimumMaximum,
		Count
	};

	enum class SeparableKernel
	{
		Gauss3x3,
		Count
	};

	//assumes subresource to initially be in state ReadCS oder ReadAny and leaves the subresource in state ReadCS
	void GenerateMipsHardwareFiltering(ID3D12GraphicsCommandList10* commandList,
		TemporaryDescriptorHeap& descriptorHeap,
		const TextureSubresource& subresource,
		SampleMode samplerMode,
		LPCWSTR passName = L"");

	//temporaryTexture expected to be in state WriteCS / UAV access
	void GenerateMipsSeparableKernel(ID3D12GraphicsCommandList10* commandList,
		TemporaryDescriptorHeap& descriptorHeap,
		const TextureSubresource& subresource,
		SeparableKernel kernel, const RWTexture& temporaryTexture,
		LPCWSTR passName = L"");

	void Init(ID3D12Device10* device); 
}

constexpr DirectX::XMUINT2 ComputeMipDimensions(const TextureProperties& textureProperties, uint32_t mipLevel)
{
	return
	{
		textureProperties.width >> mipLevel,
		textureProperties.height >> mipLevel
	};
}

constexpr DirectX::XMFLOAT2 ComputeTexelSize(const DirectX::XMUINT2& mipDimensions)
{
	return
	{
		1.0f / mipDimensions.x,
		1.0f / mipDimensions.y
	};
}

constexpr DirectX::XMFLOAT2 ComputeMipTexelSize(const TextureProperties& textureProperties, uint32_t mipLevel)
{
	return ComputeTexelSize(ComputeMipDimensions(textureProperties, mipLevel));
}

DescriptorHeap::Id CreateMipSubresource(TemporaryDescriptorHeap& descriptorHeap, const TextureResource& resource, uint32_t mipLevel); 
