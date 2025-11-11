#include "stdafx.h"
#include "DepthBuffer.h"

#include "D3DUtility.h"

static D3D12_DSV_DIMENSION GetDsvDimension(const TextureProperties& textureProperties);

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

void DepthBuffer::Bind(ID3D12GraphicsCommandList10* commandList, D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle, uint32_t depthBufferArrayIndex) const
{
	D3D12_VIEWPORT viewports = { .TopLeftX = 0, .TopLeftY = 0, .Width = float(properties.width), .Height = float(properties.height), .MinDepth = 0.0f, .MaxDepth = 1.0f };
	D3D12_RECT scissorRect = { .left = 0, .top = 0, .right = LONG(properties.width), .bottom = LONG(properties.height) };
	commandList->RSSetViewports(1, &viewports);
	commandList->RSSetScissorRects(1, &scissorRect);

	depthBufferArrayIndex = GetActualArrayIndex(*this, depthBufferArrayIndex);
	assert(depthBufferArrayIndex <= properties.arraySize);

	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandleCpu = dsv.Handle().Offset(depthBufferArrayIndex);
	commandList->OMSetRenderTargets(rtvHandle.ptr ? 1: 0, rtvHandle.ptr ? &rtvHandle : nullptr, true, &dsvHandleCpu);
}

void DepthBuffer::Clear(ID3D12GraphicsCommandList10* commandList, uint32_t depthBufferArrayIndex, float clearValue) const
{
	depthBufferArrayIndex = GetActualArrayIndex(*this, depthBufferArrayIndex);
	assert(depthBufferArrayIndex <= properties.arraySize);

	commandList->ClearDepthStencilView(dsv.Handle().Offset(depthBufferArrayIndex), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, clearValue, 0, 0, nullptr);
}

DsvHandle DepthBuffer::GetDsv(uint32_t depthBufferArrayIndex)
{
	depthBufferArrayIndex = GetActualArrayIndex(*this, depthBufferArrayIndex);
	assert(depthBufferArrayIndex <= properties.arraySize);

	return dsv.Handle().Offset(depthBufferArrayIndex);
}

DepthBuffer CreateDepthBuffer(ID3D12Device10* device,
	const TextureProperties& properties,
	DescriptorHeap& srvHeap,
	LPCWSTR name,
	TextureUsage usage,
	TextureMemoryType memoryType,
	D3D12_BARRIER_LAYOUT initialLayout,
	const D3D12_CLEAR_VALUE* clearValue,
	TextureArrayType arrayType)
{
	DepthBuffer result;
	static_cast<TextureResource&>(result) = CreateTextureResource(device,
			properties,
			TextureType::DepthBuffer,
			usage,
			name,
			memoryType,
			initialLayout,
			clearValue);

	DsvHeap::Allocation dsv;
	if (IsArray(properties))
	{
		//for arrays a dsv heap handle is created for each element and another one for all array elements at the end
		dsv = D3D::dsvHeap.Allocate(properties.arraySize + 1);
		for (uint32_t i = 0; i < properties.arraySize; i++)
		{
			D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = GetDefaultDsvDesc(properties);
			dsvDesc.Texture2DArray.ArraySize = 1; 
			dsvDesc.Texture2DArray.FirstArraySlice = i;

			device->CreateDepthStencilView(result.ptr.Get(), &dsvDesc, dsv.Handle().Offset(i));
		}
		device->CreateDepthStencilView(result.ptr.Get(), nullptr, dsv.Handle().Offset(properties.arraySize));
	}
	else
	{
		dsv = D3D::dsvHeap.Allocate();
		device->CreateDepthStencilView(result.ptr.Get(), nullptr, dsv.Handle());
	}

	result.dsv= dsv;

	switch (usage)
	{
	case TextureUsage::ReadWrite:
		assert(false);
		result.uavId = CreateUavOnHeap(srvHeap, result);
		[[fallthrough]];
	case TextureUsage::Default:
		result.srvId = CreateSrvOnHeap(srvHeap, result, arrayType); 
		break;
	default:
		break;
	}

	return result;
}

D3D12_DSV_DIMENSION GetDsvDimension(const TextureProperties& textureProperties)
{
	bool isMultiSample = textureProperties.sampleCount > 1; 
	bool isArray = IsArray(textureProperties);
	D3D12_RESOURCE_DIMENSION dimension = GetResourceDimension(textureProperties);

	switch (dimension)
	{
	case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
		return isArray ? D3D12_DSV_DIMENSION_TEXTURE1D : D3D12_DSV_DIMENSION_TEXTURE1D;
	case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
		return isArray ? D3D12_DSV_DIMENSION_TEXTURE2DARRAY : D3D12_DSV_DIMENSION_TEXTURE2D;
	case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
		assert(false); //@note: no 3d depth targets
		return {};
	default:
		assert(false);
		return {};
	}
}

D3D12_DEPTH_STENCIL_VIEW_DESC GetDefaultDsvDesc(const TextureProperties& textureProperties)
{
	D3D12_DSV_DIMENSION viewDimension = GetDsvDimension(textureProperties);

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{
		.Format = textureProperties.format ,
		.ViewDimension = viewDimension,
		.Flags = D3D12_DSV_FLAG_NONE
	};

	switch (viewDimension)
	{
	case D3D12_DSV_DIMENSION_TEXTURE2D:
		dsvDesc.Texture2D = {
			.MipSlice = 0,
		};
		break;
	case D3D12_DSV_DIMENSION_TEXTURE2DARRAY:
		dsvDesc.Texture2DArray = {
			.MipSlice = 0,
			.FirstArraySlice = 0,
			.ArraySize = textureProperties.arraySize,
		};
		break;
	default:
		assert(false);
		break;
	}

	return dsvDesc;
}

PingPong<DepthBuffer> CreatePingPongDepthBuffer(ID3D12Device10* device,
	const TextureProperties& properties,
	DescriptorHeap& srvHeap,
	LPCWSTR name,
	TextureUsage usage)
{
	PingPong<DepthBuffer> depthBuffer;

	wchar_t nameBuffer[128];
	swprintf(nameBuffer,  _countof(nameBuffer),L"%s0", name);
	depthBuffer.Current() = CreateDepthBuffer(device,
		properties,
		srvHeap,
		nameBuffer,
		usage,
		TextureMemoryType::Default,
		D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE);

	swprintf(nameBuffer, _countof(nameBuffer), L"%s1", name);
	depthBuffer.Other() = CreateDepthBuffer(device,
		properties,
		srvHeap,
		nameBuffer,
		usage,
		TextureMemoryType::Default,
		D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_SHADER_RESOURCE);

	return depthBuffer;
}
