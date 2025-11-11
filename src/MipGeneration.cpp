#include "stdafx.h"
#include "MipGeneration.h"

#include "Texture.h"
#include "DescriptorHeap.h"

namespace MipGenerator 
{
	static ComPtr<ID3D12PipelineState> hardwareFilteringPsos[uint32_t(SampleMode::Count)];
	static ComPtr<ID3D12PipelineState> separableKernelPsos[uint32_t(SeparableKernel::Count)];
	static ID3D12Device10* devicePtr;

	void GenerateMipsHardwareFiltering(ID3D12GraphicsCommandList10* commandList,
		TemporaryDescriptorHeap& descriptorHeap,
		const TextureSubresource& subresource,
		SampleMode samplerMode,
		LPCWSTR passName)
	{
		const TextureResource& parentResource = *subresource.parentResourcePtr;

		PIXScopedEvent(commandList, PIX_COLOR_DEFAULT, passName);
		commandList->SetPipelineState(hardwareFilteringPsos[uint32_t(samplerMode)].Get());

		for (uint32_t arrayIndex = subresource.range.arrayRange.begin; arrayIndex < subresource.range.arrayRange.End(); arrayIndex++)
		{
			const DirectX::XMUINT2 dimensions = ComputeMipDimensions(parentResource.properties, subresource.range.mipRange.begin);
			uint32_t outputWidth = dimensions.x;
			uint32_t outputHeight = dimensions.y;

			D3D12_TEXTURE_BARRIER barrierBackTransition{};
			PIXScopedEvent(commandList, PIX_COLOR_DEFAULT, "Array Element: %u", arrayIndex);
			for (uint32_t mipIndex = subresource.range.mipRange.begin; mipIndex < subresource.range.mipRange.End() - 1; mipIndex++)
			{
				PIXScopedEvent(commandList, PIX_COLOR_DEFAULT, "Mip: %u", mipIndex + 1);
				TextureSubresource subresourceInput = subresource;
				subresourceInput.range.mipRange.begin = mipIndex;
				subresourceInput.range.mipRange.count = 1;
				subresourceInput.range.arrayRange.begin = arrayIndex;
				subresourceInput.range.arrayRange.count = 1;
				DescriptorHeap::Id srvId = CreateSrvOnHeap(descriptorHeap, subresourceInput);

				TextureSubresource subresourceOutput = subresourceInput;
				subresourceOutput.range.mipRange.begin = mipIndex + 1;
				DescriptorHeap::Id uavId = CreateUavOnHeap(descriptorHeap, subresourceOutput);

				ResourceTransitions(commandList,
					{
						BarrierCSReadToWrite(subresourceOutput),
						barrierBackTransition
					});

				const uint32_t inputWidth = outputWidth;
				const uint32_t inputHeight = outputHeight;
				outputWidth /= 2;
				outputHeight /= 2;

				outputWidth = Max(outputWidth, 1u);
				outputHeight = Max(outputHeight, 1u);

				const float outputTexelWidth = 1.0f / outputWidth;
				const float outputTexelHeight = 1.0f / outputHeight;

				DispatchComputePass(commandList,
					nullptr,
					{ .dispatchX = outputWidth, .dispatchY = outputHeight },
					srvId,
					uavId,
					outputTexelWidth, 
					outputTexelHeight,
					inputWidth,
					inputHeight);

				barrierBackTransition = BarrierCSWriteToRead(subresourceOutput);
			}

			ResourceTransitions(commandList, { barrierBackTransition });
		}
	}

	void GenerateMipsSeparableKernel(ID3D12GraphicsCommandList10* commandList,
		TemporaryDescriptorHeap& descriptorHeap,
		const TextureSubresource& subresource,
		SeparableKernel kernel,
		const RWTexture& temporaryTexture,
		LPCWSTR passName)
	{
		const TextureResource& parentResource = *subresource.parentResourcePtr;

		PIXScopedEvent(commandList, PIX_COLOR_DEFAULT, passName);
		commandList->SetPipelineState(separableKernelPsos[uint32_t(SeparableKernel::Gauss3x3)].Get());


		for (uint32_t arrayIndex = subresource.range.arrayRange.begin; arrayIndex < subresource.range.arrayRange.End(); arrayIndex++)
		{
			const DirectX::XMUINT2 dimensions = ComputeMipDimensions(parentResource.properties, subresource.range.mipRange.begin);
			uint32_t width = dimensions.x;
			uint32_t height = dimensions.y;

			PIXScopedEvent(commandList, PIX_COLOR_DEFAULT, "Array Element: %u", arrayIndex);
			for (uint32_t mipIndex = subresource.range.mipRange.begin; mipIndex < subresource.range.mipRange.End() - 1; mipIndex++)
			{
				PIXScopedEvent(commandList, PIX_COLOR_DEFAULT, "Mip: %u", mipIndex + 1);
				TextureSubresource subresourceTemporary = temporaryTexture.GetSubresource({ .mipRange = {.begin = mipIndex } });
				DescriptorHeap::Id uavIdTemporary = CreateUavOnHeap(descriptorHeap, subresourceTemporary);
				DescriptorHeap::Id srvIdTemporary = CreateSrvOnHeap(descriptorHeap, subresourceTemporary);

				TextureSubresource subresourceInput = subresource;
				subresourceInput.range.mipRange.begin = mipIndex;
				subresourceInput.range.mipRange.count = 1;
				subresourceInput.range.arrayRange.begin = arrayIndex;
				subresourceInput.range.arrayRange.count = 1;
				DescriptorHeap::Id srvId = CreateSrvOnHeap(descriptorHeap, subresourceInput);

				TextureSubresource subresourceOutput = subresourceInput;
				subresourceOutput.range.mipRange.begin = mipIndex + 1;
				subresourceOutput.range.mipRange.count = 1;
				DescriptorHeap::Id uavId = CreateUavOnHeap(descriptorHeap, subresourceOutput);

				//vertical pass without downsampling
				{
					const bool isHorizontal = false;
					DispatchComputePass(commandList,
						nullptr,
						{ .dispatchX = width, .dispatchY = height },
						srvId,
						uavIdTemporary,
						1.0f / width,
						1.0f / height,
						isHorizontal);
				}

				ResourceTransitions(commandList,
					{
						BarrierCSReadToWrite(subresourceOutput),
						BarrierCSWriteToRead(subresourceTemporary)
					});

				width /= 2;
				height /= 2;

				width = Max(width, 1u);
				height = Max(height, 1u);

				//horizontal pass with downsampling
				{
					const bool isHorizontal = true;
					DispatchComputePass(commandList,
						nullptr,
						{ .dispatchX = width, .dispatchY = height },
						srvIdTemporary,
						uavId,
						1.0f / width,
						1.0f / height,
						static_cast<uint32_t>(isHorizontal));
				}

				ResourceTransitions(commandList, { BarrierCSWriteToRead(subresourceOutput) });
			}
			//transition back to unordered access for reuse
			TextureSubresource subresourceTemporary = temporaryTexture.GetSubresource({ .mipRange = {.begin = subresource.range.mipRange.begin, .count = subresource.range.mipRange.count - 1 } });
			ResourceTransitions(commandList, { BarrierCSReadToWrite(subresourceTemporary) });
		}
	}

	void Init(ID3D12Device10* device)
	{
		hardwareFilteringPsos[uint32_t(SampleMode::Bilinear)] = CreateComputePso(device, {.cs = LoadShaderBinary(L"content\\shaderbinaries\\MipMapDownsampleBilinear.cso").Get()});
		hardwareFilteringPsos[uint32_t(SampleMode::Minimum)] = CreateComputePso(device, {.cs = LoadShaderBinary(L"content\\shaderbinaries\\MipMapDownsampleMinimum.cso").Get()});
		hardwareFilteringPsos[uint32_t(SampleMode::Maximum)] = CreateComputePso(device, {.cs = LoadShaderBinary(L"content\\shaderbinaries\\MipMapDownsampleMaximum.cso").Get()});
		hardwareFilteringPsos[uint32_t(SampleMode::MinimumMaximum)] = CreateComputePso(device, {.cs = LoadShaderBinary(L"content\\shaderbinaries\\MipMapDownsampleMinimumMaximum.cso").Get()});

		separableKernelPsos[uint32_t(SeparableKernel::Gauss3x3)] = CreateComputePso(device, {.cs = LoadShaderBinary(L"content\\shaderbinaries\\MipMapDownsampleGauss3x3.cso").Get()});

		devicePtr = device;
	}
}

DescriptorHeap::Id CreateMipSubresource(TemporaryDescriptorHeap& descriptorHeap, const TextureResource& resource, uint32_t mipLevel)
{
	return CreateUavOnHeap(descriptorHeap, resource.GetSubresource({ .mipRange = { .begin = mipLevel } }));
}
