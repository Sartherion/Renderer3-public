#pragma once

struct DescriptorHeap;
struct BufferHeap;
struct Texture;

namespace BlueNoiseGeneration
{
	inline const Texture* texture;

	void Init(ID3D12Device10* device, DescriptorHeap& descriptorHeap, BufferHeap& bufferHeap);

	void Generate(ID3D12GraphicsCommandList10* commandList);

	[[nodiscard]]
	D3D12_TEXTURE_BARRIER Done();
}

//@note: adapted from https://www.shadertoy.com/view/3dyyR3
constexpr float HaltonSequence(int base, int index)
{
	float result = 0.;
	float f = 1.;
	while (index > 0)
	{
		f = f / float(base);
		result += f * float(index % base);
		index = index / base;
	}
	return result;
}

