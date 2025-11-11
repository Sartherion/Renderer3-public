#include "stdafx.h"
#include "D3DUtility.h"

#include "SharedDefines.h"
#include "Texture.h"

static 	ComPtr<ID3D12PipelineState> bufferClearPso;
static 	ComPtr<ID3D12PipelineState> textureClearPso;

void InitializeUtility(ID3D12Device10* device)
{
	bufferClearPso = CreateComputePso(device, { .cs = LoadShaderBinary(L"content\\shaderbinaries\\BufferClearCS.cso").Get() });
	textureClearPso = CreateComputePso(device, { .cs = LoadShaderBinary(L"content\\shaderbinaries\\TextureClearCS.cso").Get() });
}

ComPtr<IDxcBlobEncoding> LoadShaderBinary(LPCWSTR filename)
{
	ComPtr<IDxcUtils> utility;
	DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utility));
	ComPtr<IDxcBlobEncoding> source = nullptr;
	if (FAILED(utility->LoadFile(filename, nullptr, &source)))
	{
		std::wstring errorMessage = std::wstring(L"Could not open file: ") + filename + L"\n";
		OutputDebugString(errorMessage.c_str());
	}
	return source;
}


void DumpToFile(LPCWSTR name, const void* dataPtr, size_t size)
{
    FILE* file;

    file = _wfopen(name, L"wb");

    fwrite(dataPtr, size, 1, file);

    fclose(file);
}

ComPtr<ID3D12PipelineState> CreateGraphicsPso(ID3D12Device10* device, const GraphicsPsoDesc&& desc)
{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature = desc.rootSignature;
		if (desc.vs)
		{
			psoDesc.VS = { desc.vs->GetBufferPointer(), desc.vs->GetBufferSize() };
		}
		if (desc.ps)
		{
			psoDesc.PS = { desc.ps->GetBufferPointer(), desc.ps->GetBufferSize() };
		}
		if (desc.ds)
		{
			psoDesc.DS = { desc.ds->GetBufferPointer(), desc.ds->GetBufferSize() };
		}
		if (desc.hs)
		{
			psoDesc.HS = { desc.hs->GetBufferPointer(), desc.hs->GetBufferSize() };
		}
		if (desc.gs)
		{
			psoDesc.GS = { desc.gs->GetBufferPointer(), desc.gs->GetBufferSize() };
		}
		psoDesc.RasterizerState = desc.rasterizerState;
		psoDesc.BlendState = desc.blendState;
		psoDesc.DepthStencilState = desc.depthState;
		psoDesc.SampleMask = desc.sampleMask;
		psoDesc.PrimitiveTopologyType = desc.primitiveTopologyType;
		psoDesc.NumRenderTargets = static_cast<uint32_t>(desc.rtvFormats.size());
		for (int i = 0; DXGI_FORMAT rtvFormat : desc.rtvFormats)
		{
			psoDesc.RTVFormats[i++] = rtvFormat;
		}
		psoDesc.DSVFormat = desc.dsvFormat;
		psoDesc.SampleDesc = desc.sampleDesc;
		psoDesc.InputLayout = desc.inputLayout;

		ComPtr<ID3D12PipelineState> pso;
		CheckForErrors(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso)));

		return pso;
}

ComPtr<ID3D12PipelineState> CreateComputePso(ID3D12Device10* device, const ComputePsoDesc&& desc)
{
    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
	if (desc.cs)
	{
		psoDesc.CS = { desc.cs->GetBufferPointer(), desc.cs->GetBufferSize() };
	}
    psoDesc.pRootSignature = desc.rootSignature;

	ComPtr<ID3D12PipelineState> pso;
	CheckForErrors(device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&pso)));

    return pso;
}

void ClearBufferUav(ID3D12GraphicsCommandList10* commandList,
	DescriptorHeap::Id uavId,
	uint32_t value,
	uint32_t elementCount,
	uint32_t elementsPerThreadCount)
{
	commandList->SetPipelineState(bufferClearPso.Get());
	commandList->SetComputeRoot32BitConstant(0, uavId, 0);
	commandList->SetComputeRoot32BitConstant(0, value, 1);
	commandList->SetComputeRoot32BitConstant(0, elementsPerThreadCount, 2); 

	const uint32_t dispatchSize = DivisionRoundUp(elementCount, elementsPerThreadCount * bufferClearThreadGroupSize);
	commandList->SetComputeRoot32BitConstant(0, dispatchSize, 3);
	commandList->Dispatch(dispatchSize, 1, 1); 
}

void ClearRWTexture(ID3D12GraphicsCommandList10* commandList, const RWTexture& texture, uint32_t (&values)[4])
{
	commandList->SetPipelineState(textureClearPso.Get());
	commandList->SetComputeRoot32BitConstant(0, texture.uavId, 0);
	commandList->SetComputeRoot32BitConstants(0, 4, values, 1);

	const uint32_t dispatchSizeX = DivisionRoundUp(texture.properties.width, textureClearThreadGroupSizeX);
	const uint32_t dispatchSizeY = DivisionRoundUp(texture.properties.height, textureClearThreadGroupSizeY);
	commandList->Dispatch(dispatchSizeX, dispatchSizeY, 1); 
}

void WriteImmediateValue(ID3D12GraphicsCommandList10* commandList, D3D12_GPU_VIRTUAL_ADDRESS address, uint32_t value)
{
	D3D12_WRITEBUFFERIMMEDIATE_PARAMETER writeImmediateParameter =
	{
		.Dest = address,
		.Value = value
	};

	commandList->WriteBufferImmediate(1, &writeImmediateParameter, nullptr);
}

D3D12_BLEND_DESC GetBlendState(BlendState blendState)
{
	static const D3D12_BLEND_DESC blendDescs[uint32_t(BlendState::Count)] =
	{
		// BlendState::Disabled
		{
			.RenderTarget =
			{
				{
					.BlendEnable = false,
					.SrcBlend = D3D12_BLEND_SRC_ALPHA,
					.DestBlend = D3D12_BLEND_INV_SRC_ALPHA,
					.BlendOp = D3D12_BLEND_OP_ADD,
					.SrcBlendAlpha = D3D12_BLEND_ONE,
					.DestBlendAlpha = D3D12_BLEND_ONE,
					.BlendOpAlpha = D3D12_BLEND_OP_ADD,
					.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL
				}
			}
		},
		//BlendState::Additive
		{
			.RenderTarget =
			{
				{
			        .BlendEnable = true,
			        .SrcBlend = D3D12_BLEND_ONE,
			        .DestBlend = D3D12_BLEND_ONE,
			        .BlendOp = D3D12_BLEND_OP_ADD,
			        .SrcBlendAlpha = D3D12_BLEND_ONE,
			        .DestBlendAlpha = D3D12_BLEND_ONE,
			        .BlendOpAlpha = D3D12_BLEND_OP_ADD,
			        .RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL,
				}
			}
		},
		//BlendState::AlpheBlend
		{
			.RenderTarget =
			{
				{
					.BlendEnable = true,
					.SrcBlend = D3D12_BLEND_SRC_ALPHA,
					.DestBlend = D3D12_BLEND_INV_SRC_ALPHA,
					.BlendOp = D3D12_BLEND_OP_ADD,
					.SrcBlendAlpha = D3D12_BLEND_ONE,
					.DestBlendAlpha = D3D12_BLEND_ONE,
					.BlendOpAlpha = D3D12_BLEND_OP_ADD,
					.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL,
				}
			}
		}
	};

    return blendDescs[uint32_t(blendState)];
}

D3D12_RASTERIZER_DESC GetRasterizerState(RasterizerState rasterizerState)
{
	static const D3D12_RASTERIZER_DESC rasterizerDescs[uint32_t(RasterizerState::Count)] =
	{
		//RasterizerState::NoCull
		{
			.FillMode = D3D12_FILL_MODE_SOLID,
			.CullMode = D3D12_CULL_MODE_NONE,
			.DepthClipEnable = true,
			.MultisampleEnable = true
		},
		//RasterizerState::BackFaceCull
		{
			.FillMode = D3D12_FILL_MODE_SOLID,
			.CullMode = D3D12_CULL_MODE_BACK,
			.DepthClipEnable = true,
			.MultisampleEnable = true
		},
		//RasterizerState::FrontFaceCull
		{
			.FillMode = D3D12_FILL_MODE_SOLID,
			.CullMode = D3D12_CULL_MODE_FRONT,
			.DepthClipEnable = true,
			.MultisampleEnable = true
		},
		//RasterizerState::Wireframe
		{
			.FillMode = D3D12_FILL_MODE_WIREFRAME,
			.CullMode = D3D12_CULL_MODE_NONE,
			.DepthClipEnable = true,
			.MultisampleEnable = true
		}
	};

    return rasterizerDescs[uint32_t(rasterizerState)];
}

D3D12_DEPTH_STENCIL_DESC GetDepthState(DepthState depthState)
{
	static const D3D12_DEPTH_STENCIL_DESC depthDescs[uint32_t(DepthState::Count)] =
	{
		//DepthState::Disabled
		{
			.DepthEnable = false,
			.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO,
			.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL,
		},
		//DepthState::Test
		{
			.DepthEnable = true,
			.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO,
			.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL,
		},
		//DepthState::TestEqual
		{
			.DepthEnable = true,
			.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO,
			.DepthFunc = D3D12_COMPARISON_FUNC_EQUAL,
		},
		//DepthState::Write
		{
			.DepthEnable = true,
			.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL,
			.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL,
		}
	};

	return depthDescs[uint32_t(depthState)];
}

void TemporaryRWTexturePool::Init(ID3D12Device10* device,
	DescriptorHeap& descriptorHeap,
	const TextureProperties& properties,
	uint32_t elementCount,
	LPCWSTR name)
{
	textures = new RWTexture[elementCount];
	available = new RWTexture * [elementCount];

	for (uint32_t i = 0; i < elementCount; i++)
	{
		textures[i] = CreateRWTexture(device, properties, descriptorHeap, name);
		available[i] = &textures[i];
	}
	availableCount = elementCount;
}

void TemporaryRWTexturePool::Free()
{
	delete[] textures;
	delete[] available;
}

RWTexture& TemporaryRWTexturePool::Acquire()
{
	assert(availableCount > 0);
	return *available[--availableCount];
}

void TemporaryRWTexturePool::Release(RWTexture& temporaryTexture)
{
	available[availableCount++] = &temporaryTexture;
}
