#include "stdafx.h"
#include "PostProcess.h"

#include "D3DUtility.h"
#include "DescriptorHeap.h"
#include "MipGeneration.h"
#include "RenderTarget.h"

namespace PostProcess
{
	static const int bloomDownSampleLevelMaxCount = 5;

	static ComPtr<ID3D12PipelineState> bloomUpPso;
	static ComPtr<ID3D12PipelineState> bloomPrefilterFirefliesPso;
	static ComPtr<ID3D12PipelineState> toneMapPso;

	static void InitializePsos(ID3D12Device10* device, DXGI_FORMAT displayFormat);

	static DirectX::XMFLOAT4 CalculateBloomThreshold(float bloomThreshold, float bloomThresholdKnee);
	static DirectX::XMFLOAT4 CalculateColorAdjustments(float postExposure, float contrast, float hueShift, float saturation);

	void Init(ID3D12Device10* device, DXGI_FORMAT displayFormat) 
	{
		InitializePsos(device, displayFormat);
	}

	void RenderBloom(ID3D12GraphicsCommandList10* commandList, 
		TemporaryDescriptorHeap& descriptorHeap, 
		const Texture& inputBuffer, 
		RWTexture& outputBuffer, 
		RWTexture& temporaryBuffer0, 
		RWTexture& temporaryBuffer1, 
		const UI::BloomSettings& settings)
	{
		PIXScopedEvent(commandList, PIX_COLOR_DEFAULT, "Bloom");
		PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, "Downsampling");
		const DirectX::XMFLOAT4 threshold = PostProcess::CalculateBloomThreshold(settings.threshold, settings.thresholdKnee);
		const DirectX::XMUINT2 outputDimensions = ComputeMipDimensions(temporaryBuffer0.properties, 1);
		const DirectX::XMFLOAT2 outputTexelSize = ComputeTexelSize(outputDimensions);

		DispatchComputePass(commandList,
			bloomPrefilterFirefliesPso.Get(),
			{ .dispatchX = outputDimensions.x, .dispatchY = outputDimensions.y }, 
			CreateMipSubresource(descriptorHeap, temporaryBuffer0, 1),
			inputBuffer.srvId.Id(),
			outputTexelSize,
			threshold);

		ResourceTransitions(commandList, { BarrierCSWriteToRead(temporaryBuffer0) });

		MipGenerator::GenerateMipsSeparableKernel(commandList, 
			descriptorHeap, 
			temporaryBuffer0.GetSubresource({ .mipRange = { .begin = 1, .count = bloomDownSampleLevelMaxCount } }),
			MipGenerator::SeparableKernel::Gauss3x3, 
			temporaryBuffer1);
		PIXEndEvent(commandList);

		PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, "Upsampling");
		commandList->SetPipelineState(bloomUpPso.Get());

		ResourceTransitions(commandList, { BarrierCSWriteToRead(temporaryBuffer1) });

		D3D12_TEXTURE_BARRIER barrierBackTransition = {};
		for (int i = bloomDownSampleLevelMaxCount - 1; i >= 0; i--)
		{
			const bool isFirstIteration = i == bloomDownSampleLevelMaxCount - 1;
			const bool isLastIteration = i == 0;

			DescriptorHeap::Id lowResolutionInputSrvId = CreateMipSubresource(descriptorHeap, isFirstIteration ? temporaryBuffer0 : temporaryBuffer1, i + 1);

			DescriptorHeap::Id highResolutionOutputUavId;
			DescriptorHeap::Id highResolutionInputSrvId;
			DirectX::XMUINT2 outputDimensions;
			if (!isLastIteration)
			{
				highResolutionInputSrvId = CreateMipSubresource(descriptorHeap, temporaryBuffer0, i);

				TextureSubresource destination = temporaryBuffer1.GetSubresource({ .mipRange = {.begin = static_cast<uint32_t>(i) } }); 
				highResolutionOutputUavId = CreateUavOnHeap(descriptorHeap, destination);

				outputDimensions = ComputeMipDimensions(destination.parentResourcePtr->properties, i);

				ResourceTransitions(commandList, 
					{
						BarrierCSReadToWrite(destination),
						barrierBackTransition
					});
				barrierBackTransition = BarrierCSWriteToRead(destination);
			}
			else
			{
				highResolutionInputSrvId = inputBuffer.srvId;
				highResolutionOutputUavId = outputBuffer.uavId;
				outputDimensions = ComputeMipDimensions(outputBuffer.properties, 0);
				ResourceTransitions(commandList, { barrierBackTransition });
			}

			const DirectX::XMFLOAT2 outputTexelSize = ComputeTexelSize(outputDimensions);

			DispatchComputePass(commandList,
				nullptr,
				{ .dispatchX = outputDimensions.x, .dispatchY = outputDimensions.y },
				highResolutionOutputUavId,
				highResolutionInputSrvId,
				lowResolutionInputSrvId,
				settings.intensity,
				outputTexelSize);
		}

		ResourceTransitions(commandList, 
			{
				BarrierCSReadToWrite(temporaryBuffer1),
				BarrierCSReadToWrite(temporaryBuffer0)
			});

		PIXEndEvent(commandList);
	}

	DirectX::XMFLOAT4 CalculateBloomThreshold(float bloomThreshold, float bloomThresholdKnee)
	{
		DirectX::XMFLOAT4 threshold;
		threshold.x = SrgbToLinear(bloomThreshold);
		threshold.y = threshold.x * bloomThresholdKnee;
		threshold.z = 2.0f * threshold.y;
		threshold.w = 0.25f / (threshold.y + 0.00001f);
		threshold.y -= threshold.x;
		return threshold;
	};

	DirectX::XMFLOAT4 CalculateColorAdjustments(float postExposure, float contrast, float hueShift, float saturation)
	{
		return
		{
			std::pow(2.0f, postExposure),
			contrast * 0.01f + 1.0f,
			hueShift * (1.0f / 360.0f),
			saturation * 0.01f + 1.0f
		};
	};

	void InitializePsos(ID3D12Device10* device, DXGI_FORMAT displayFormat)
	{
		//tone mapping
		toneMapPso = CreateGraphicsPso(device,
			{
				.vs = LoadShaderBinary(L"content\\shaderbinaries\\FullscreenVS.cso").Get(),
				.ps = LoadShaderBinary(L"content\\shaderbinaries\\ToneMappingPS.cso").Get(),
				.depthState = GetDepthState(DepthState::Disabled),
				.rtvFormats = { { displayFormat } },

			});

		//bloom 
		bloomUpPso = CreateComputePso(device, { .cs = LoadShaderBinary(L"content\\shaderbinaries\\BloomUpCS.cso").Get()});
		bloomPrefilterFirefliesPso = CreateComputePso(device, { .cs = LoadShaderBinary(L"content\\shaderbinaries\\BloomPrefilterFirefliesCS.cso").Get()});
	}

	void ToneMap(ID3D12GraphicsCommandList10* commandList, 
		ScratchHeap& bufferHeap, 
		const Texture& inputBuffer, 
		const RenderTarget& outputRT, 
		const UI::ColorGradingSettings& settings)
	{
		struct ColorGradingData
		{
			DirectX::XMFLOAT4 colorAdjustments;
			DirectX::XMFLOAT4 colorFilter;
		} colorGradingData;

		colorGradingData.colorAdjustments = PostProcess::CalculateColorAdjustments(settings.postExposure, settings.contrast, settings.hueShift, settings.saturation);
		colorGradingData.colorFilter = settings.color;

		PIXScopedEvent(commandList, PIX_COLOR_DEFAULT, "Tone Mapping");
		outputRT.Bind(commandList);
		commandList->SetPipelineState(toneMapPso.Get());
		BindGraphicsRootConstants(commandList,
			inputBuffer.srvId.Id(),
			WriteTemporaryData(bufferHeap, colorGradingData));
		commandList->DrawInstanced(3, 1, 0, 0);
	}
}

void UI::PostProcessSettings::MenuEntry()
{
	if (ImGui::CollapsingHeader("Post Process Settings", ImGuiTreeNodeFlags_None))
	{
		ImGui::SeparatorText("Bloom Settings");
		ImGui::DragFloat("Intensity", &bloomSettings.intensity, 0.005f, 0.0f, FLT_MAX, "%.1f");
		ImGui::DragFloat("Threshold", &bloomSettings.threshold, 0.005f, 0.0f, FLT_MAX, "%.1f");
		ImGui::DragFloat("Threshold Knee", &bloomSettings.thresholdKnee, 0.005f, 0.0f, 1.0f, "%.1f");

		ImGui::SeparatorText("Color Grading Settings");
		ImGui::DragFloat("Post Exposure", &colorGradingSettings.postExposure, 0.005f, -FLT_MAX, FLT_MAX, "%.1f");
		ImGui::SliderFloat("Contrast", &colorGradingSettings.contrast, -100.0f, 100.0f, "%.1f");
		ImGui::ColorEdit3("Color", (float*)&colorGradingSettings.color, ImGuiColorEditFlags_DisplayHSV | ImGuiColorEditFlags_HDR);
		ImGui::SliderFloat("Hue Shift", &colorGradingSettings.hueShift, -180.0f, 180.0f, "%.1f");
		ImGui::SliderFloat("Saturation", &colorGradingSettings.saturation, -180.0f, 180.0f, "%.1f");
	}
}

