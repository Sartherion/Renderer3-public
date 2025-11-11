#include "ShadowMap.h"

#include "D3DResourceHelpers.h"
#include "D3DUtility.h"


void ShadowMap::Initialize(ID3D12Device10* device, uint32_t width, uint32_t height, uint32_t count, DXGI_FORMAT format, DescriptorHeap& descriptorHeap)
{
	//each ShadowMap class gets its own dsvHeap
}

void ShadowMap::RenderBegin(ID3D12GraphicsCommandList10* commandList, uint32_t shadowMapIndex)
{
	//PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, "ShadowMapCreationPass %d", shadowMapIndex);
	//commandList->SetGraphicsRoot32BitConstant(0, shadowMapIndex, 2);
}

void ShadowMap::RenderEnd(ID3D12GraphicsCommandList10* commandList) //@todo: macht keinen sinn, dass renderbegin index nimmt und renderend nicht==> umbenennen in prepareforrender und allrenderingdone?
{
	//PIXEndEvent(commandList);
}

void ShadowMap::FrameEnd(ID3D12GraphicsCommandList10* commandList)
{
}

