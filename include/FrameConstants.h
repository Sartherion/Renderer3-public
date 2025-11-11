#pragma once
#include "BufferMemory.h"
#include "Camera.h"
#include "GBuffer.h"
#include "DDGI.h"
#include "DebugDrawing.h"
#include "IndirectDiffuse.h"
#include "Light.h"
#include "PathTracer.h"
#include "SSSR.h"
#include "SSAO.h"
#include "Texture.h"

namespace UI
{
	struct SharedSettings
	{
		MaterialSettings materialSettings;
		LightingSettings lightingSettings;
		ReferencePathTracerSettings pathtracerSettings;
		SSAOSettings ssaoSettings;
		SSSRSettings sssrSettings;
		DDGISettings ddgiSettings;
		IndirectDiffuseSettings indirectDiffuseSettings;
		DebugVisualizationSettings debugVisualizationSettings;
	};
}

struct FrameConstants
{
	Texture2DDimensions mainRenderTargetDimensions;
	Camera::Constants mainCameraData;
	Camera::Constants previousMainCameraData;
	uint32_t staticFramesCount;
	GBuffer::SrvIds gBufferSrvIds;
	DescriptorHeap::Id blueNoiseBufferSrvId;
	uint32_t blueNoiseTextureSize;
	Frame::TimingData frameTimings;
	UI::SharedSettings sharedSettings;
};

inline TemporaryBuffer<FrameConstants> UpdateFrameConstants(ID3D12GraphicsCommandList10* commandList,
	ScratchHeap& bufferHeap,
	const Camera& camera,
	const Texture2DDimensions& mainRenderTargetDimensions,
	const Texture& blueNoiseTexture,
	const GBuffer::SrvIds& gBufferSrvIds,
	const Frame::TimingData& timingData,
	const UI::SharedSettings& sharedSettings)
{
	FrameConstants frameConstants =
	{
		.mainRenderTargetDimensions = mainRenderTargetDimensions,
		.mainCameraData = camera.constants.Current(),
		.previousMainCameraData = camera.constants.Other(),
		.staticFramesCount = camera.GetStaticFramesCount(),
		.gBufferSrvIds = gBufferSrvIds,
		.blueNoiseBufferSrvId = blueNoiseTexture.srvId,
		.blueNoiseTextureSize = blueNoiseTexture.properties.width,
		.frameTimings = Frame::timingData,
		.sharedSettings = sharedSettings,
	};

	TemporaryBuffer<FrameConstants> frameConstantsBuffer = CreateTemporaryBuffer<FrameConstants>(bufferHeap);
	frameConstantsBuffer.Write(frameConstants);

	commandList->SetComputeRoot32BitConstant(2, frameConstantsBuffer.Offset(), 0);
	commandList->SetGraphicsRoot32BitConstant(2, frameConstantsBuffer.Offset(), 0);
	
	return frameConstantsBuffer;
}

