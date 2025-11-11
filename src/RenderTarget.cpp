#include "stdafx.h"
#include "RenderTarget.h"

#include "DepthBuffer.h"
#include "D3DUtility.h"
#include "Frame.h"

static D3D12_RTV_DIMENSION GetRtvDimension(const TextureProperties& textureProperties);

static uint32_t GetActualArrayIndex(const Texture& buffer, uint32_t arrayIndex)
{
	if (buffer.properties.arraySize == 1)
	{
		return 0;
	}
	if (arrayIndex == uint32_t(-1))
	{
		return buffer.properties.arraySize;
	}
	return arrayIndex;
}

void RenderTarget::Bind(ID3D12GraphicsCommandList10* commandList, D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle, uint32_t renderTargetArrayIndex) const
{
	SetViewPort(commandList, properties);
	
	renderTargetArrayIndex = GetActualArrayIndex(*this, renderTargetArrayIndex);
	assert(renderTargetArrayIndex <= properties.arraySize);

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandleCpu = rtv.Handle().Offset(renderTargetArrayIndex);
	commandList->OMSetRenderTargets(1, &rtvHandleCpu, true, dsvHandle.ptr ? &dsvHandle : nullptr);
}

void RenderTarget::Clear(ID3D12GraphicsCommandList10* commandList, uint32_t renderTargetArrayIndex, const float(&clearColor)[4]) const
{
	renderTargetArrayIndex = GetActualArrayIndex(*this, renderTargetArrayIndex);
	assert(renderTargetArrayIndex <= properties.arraySize);

	commandList->ClearRenderTargetView(rtv.Handle().Offset(renderTargetArrayIndex), clearColor, 0, nullptr);
}

RtvHandle RenderTarget::GetRtv(uint32_t renderTargetArrayIndex) const
{
	renderTargetArrayIndex = GetActualArrayIndex(*this, renderTargetArrayIndex);
	assert(renderTargetArrayIndex <= properties.arraySize);

	return rtv.Handle().Offset(renderTargetArrayIndex);
}


RenderTarget CreateRenderTarget(ID3D12Device10* device,
	const TextureProperties& properties,
	DescriptorHeap& srvHeap,
	LPCWSTR name,
	TextureUsage usage,
	TextureMemoryType memoryType,
	D3D12_BARRIER_LAYOUT initialLayout,
	const D3D12_CLEAR_VALUE* clearValue,
	TextureArrayType arrayType)
{
	RenderTarget result;
	static_cast<TextureResource&>(result) = CreateTextureResource(device,
		properties,
		TextureType::RenderTarget,
		usage,
		name,
		memoryType,
		initialLayout,
		clearValue);

	RtvHeap::Allocation rtv;
	if (IsArray(properties))
	{
		//for arrays a rtv heap handle is created for each element and another one for all array elements at the end
		rtv = D3D::rtvHeap.Allocate(properties.arraySize + 1);
		for (uint32_t i = 0; i < properties.arraySize; i++)
		{
			D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = GetDefaultRtvDesc(properties);
			rtvDesc.Texture2DArray.ArraySize = 1;  
			rtvDesc.Texture2DArray.FirstArraySlice = i;

			device->CreateRenderTargetView(result.ptr.Get(), &rtvDesc, rtv.Handle().Offset(i));
		}
		device->CreateRenderTargetView(result.ptr.Get(), nullptr, rtv.Handle().Offset(properties.arraySize));
	}
	else
	{
		rtv = D3D::rtvHeap.Allocate();
		device->CreateRenderTargetView(result.ptr.Get(), nullptr, rtv.Handle());
	}

	result.rtv = rtv;

	switch (usage)
	{
	case TextureUsage::ReadWrite:
		result.uavId = CreateUavOnHeap(srvHeap, result.ptr.Get());
		[[fallthrough]];
	case TextureUsage::Default:
		result.srvId = CreateSrvOnHeap(srvHeap, result, arrayType);
		break;
	default:
		break;
	}

	return result;
}

void DestroySafe(RenderTarget& renderTarget)
{
	Frame::SafeRelease(renderTarget.rtv);
	DestroySafe(static_cast<RWTexture&>(renderTarget));
}

D3D12_RTV_DIMENSION GetRtvDimension(const TextureProperties& textureProperties)
{
	bool isMultiSample = textureProperties.sampleCount > 1; 
	bool isArray = IsArray(textureProperties);
	D3D12_RESOURCE_DIMENSION dimension = GetResourceDimension(textureProperties);

	switch (dimension)
	{
	case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
		return isArray ? D3D12_RTV_DIMENSION_TEXTURE1D : D3D12_RTV_DIMENSION_TEXTURE1D;
	case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
		return isArray ? D3D12_RTV_DIMENSION_TEXTURE2DARRAY : D3D12_RTV_DIMENSION_TEXTURE2D;
	case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
		assert(!isArray);
		return D3D12_RTV_DIMENSION_TEXTURE3D;
	default:
		assert(false);
		return {};
	}
}

void SetViewPort(ID3D12GraphicsCommandList10* commandList, const TextureProperties& properties)
{
	D3D12_VIEWPORT viewports = { .TopLeftX = 0, .TopLeftY = 0, .Width = float(properties.width), .Height = float(properties.height), .MinDepth = 0.0f, .MaxDepth = 1.0f };
	D3D12_RECT scissorRect = { .left = 0, .top = 0, .right = LONG(properties.width), .bottom = LONG(properties.height) };
	commandList->RSSetViewports(1, &viewports);
	commandList->RSSetScissorRects(1, &scissorRect);
}

void Bind(ID3D12GraphicsCommandList10* commandList,
	std::span<const RenderTarget> renderTargets,
	const DepthBuffer& depthBuffer,
	std::span<const uint32_t> renderTargetArrayIndices,
	uint32_t depthBufferArrayIndex)
{
	const uint32_t renderTargetCount = static_cast<uint32_t>(renderTargets.size());
	assert(renderTargetCount <= 8);

	bool useRenderTargetArrayIndices = !renderTargetArrayIndices.empty();
	assert(!useRenderTargetArrayIndices || renderTargetArrayIndices.size() == renderTargetCount); //if the renderTargetArrayIndices are used, there must be as many as there are render targets

	const TextureProperties& dbProperties = depthBuffer.properties;
	SetViewPort(commandList,  dbProperties);

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[8];
	for (size_t i = 0; i < renderTargetCount; i++)
	{
		const TextureProperties& rtProperties = renderTargets[i].properties;
		assert(rtProperties.width == dbProperties.width && rtProperties.height == dbProperties.height && rtProperties.sampleCount == dbProperties.sampleCount);

		rtvHandles[i] = renderTargets[i].rtv.Handle().Offset(useRenderTargetArrayIndices ? renderTargetArrayIndices[i] : 0);
	}
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = depthBuffer.dsv.Handle().Offset(depthBufferArrayIndex).handle;
	commandList->OMSetRenderTargets(renderTargetCount, rtvHandles, false, &dsvHandle);
}

void Bind(ID3D12GraphicsCommandList10* commandList, std::span<const RenderTarget> renderTargets, std::span<const uint32_t> renderTargetArrayIndices)
{
	assert(!renderTargets.empty());

	const uint32_t renderTargetCount = static_cast<uint32_t>(renderTargets.size());
	assert(renderTargetCount <= 8);
	
	bool useRenderTargetArrayIndices = !renderTargetArrayIndices.empty();
	assert(!useRenderTargetArrayIndices || renderTargetArrayIndices.size() == renderTargetCount); //if the renderTargetArrayIndices are used, there must be as many as there are render targets


	const TextureProperties& rt0Properties = renderTargets[0].properties;
	SetViewPort(commandList,  rt0Properties);

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[8];
	for (uint32_t i = 0; i < renderTargetCount; i++)
	{
		const TextureProperties& rtProperties = renderTargets[i].properties;
		assert(rtProperties.width == rt0Properties.width && rtProperties.height == rt0Properties.height && rtProperties.sampleCount == rt0Properties.sampleCount);

		rtvHandles[i] = renderTargets[i].rtv.Handle().Offset(useRenderTargetArrayIndices ? renderTargetArrayIndices[i] : 0);
	}

	commandList->OMSetRenderTargets(renderTargetCount, rtvHandles, false, nullptr);
}

D3D12_RENDER_TARGET_VIEW_DESC GetDefaultRtvDesc(const TextureProperties& textureProperties)
{
	D3D12_RTV_DIMENSION viewDimension = GetRtvDimension(textureProperties);

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{
		.Format = textureProperties.format ,
		.ViewDimension = viewDimension,
	};

	switch (viewDimension)
	{
	case D3D12_RTV_DIMENSION_TEXTURE2D:
		rtvDesc.Texture2D = {
			.MipSlice = 0,
			.PlaneSlice = 0
		};
		break;
	case D3D12_RTV_DIMENSION_TEXTURE2DARRAY:
		rtvDesc.Texture2DArray = {
			.MipSlice = 0,
			.FirstArraySlice = 0,
			.ArraySize = textureProperties.arraySize,
			.PlaneSlice = 0
		};
		break;
	default:
		assert(false);
		break;
	}

	return rtvDesc;
}

DepthBuffer CreateDepthBuffer(ID3D12Device10* device,
	const TextureProperties& properties,
	DescriptorHeap& srvHeap,
	LPCWSTR name,
	TextureUsage usage,
	TextureMemoryType memoryType,
	const D3D12_CLEAR_VALUE* clearValue,
	D3D12_BARRIER_LAYOUT initialLayout,
	TextureArrayType arrayType)
{
	return DepthBuffer();
}

void DestroySafe(DepthBuffer& depthBuffer)
{
	Frame::SafeRelease(depthBuffer.dsv);
	DestroySafe(static_cast<RWTexture&>(depthBuffer));
}

PingPong<RenderTarget> CreatePingPongRenderTarget(ID3D12Device10* device,
	const TextureProperties& properties,
	DescriptorHeap& srvHeap,
	LPCWSTR name,
	TextureUsage usage)
{
	PingPong<RenderTarget> renderTarget;

	wchar_t nameBuffer[128];
	swprintf(nameBuffer,  _countof(nameBuffer),L"%s0", name);
	renderTarget.Current() = CreateRenderTarget(device, properties, srvHeap, nameBuffer, usage, TextureMemoryType::Default , D3D12_BARRIER_LAYOUT_RENDER_TARGET);
	swprintf(nameBuffer, _countof(nameBuffer), L"%s1", name);
	renderTarget.Other() = CreateRenderTarget(device, properties, srvHeap, nameBuffer, usage, TextureMemoryType::Default, D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_SHADER_RESOURCE);

	return renderTarget;
}