#pragma once

struct RenderTarget;
struct Texture;
struct RWTexture;
struct TemporaryDescriptorHeap;
struct ScratchHeap;

namespace UI
{
	struct BloomSettings;
	struct ColorGradingSettings;
}

namespace PostProcess
{
	void Init(ID3D12Device10* device, DXGI_FORMAT displayFormat);

	void RenderBloom(ID3D12GraphicsCommandList10* commandList,
		TemporaryDescriptorHeap& descriptorHeap,
		const Texture& inputBuffer, // expected state: ReadAny/ReadCS
		RWTexture& outputBuffer, // expected state: WriteCS; final state: WriteCS
		RWTexture& temporaryBuffer0, // expected state: WriteCS; final state: WriteCS
		RWTexture& temporaryBuffer1, // expected state: WriteCS; final state: WriteCS
		const UI::BloomSettings& settings);
}

namespace PostProcess
{
	void ToneMap(ID3D12GraphicsCommandList10* commandList, 
		ScratchHeap& bufferHeap, 
		const Texture& inputBuffer, 
		const RenderTarget& outputRT, 
		const UI::ColorGradingSettings& settings);
}

namespace UI
{
	struct BloomSettings
	{
		float intensity = 0.4f;
		float threshold = 1.0f;
		float thresholdKnee = 0.5f;
	};
	
	struct ColorGradingSettings
	{
		float postExposure = 0.0f;
		float contrast = 0.0f;
		DirectX::XMFLOAT4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
		float hueShift = 0.0f;
		float saturation = 0.0f;
	};

	struct PostProcessSettings
	{
		BloomSettings bloomSettings;
		ColorGradingSettings colorGradingSettings;

		void MenuEntry();
	};
}

