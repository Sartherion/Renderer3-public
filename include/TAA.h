#pragma once

struct DescriptorHeap;
struct Texture;

namespace TAA
{
	void Init(ID3D12Device10* device, DXGI_FORMAT format, uint32_t width, uint32_t height, DescriptorHeap& descriptorHeap);

	//expects input to be in ReadCS or ReadAny state
	Texture& Resolve(ID3D12GraphicsCommandList10* commandList, Texture& input, bool isActive = true);
}

namespace UI
{
	struct TAASettings 
	{
		bool useTaa = true;
		void MenuEntry();
	};
}
