#pragma once 
#include "Common.hlsli"

#include "GBuffers.hlsli"
#include "UISettings.hlsli"

struct FrameTimings
{
	uint64_t frameId;
	double elapsedTimeMs;
	float deltaTimeMs;
	float averageDeltaTimeMs;
};

struct FrameConstants
{
    TextureDimensions mainRenderTargetDimensions;
    CameraConstants mainCameraData;
    CameraConstants previousMainCameraData;
    uint staticFramesCount;
    GBufferSrvIds gBufferSrvIds;
    uint blueNoiseBufferSrvId;
    uint blueNoiseTextureSize;
    FrameTimings frameTimings;
    UISettings settings;
};

struct FrameConstantsOffset
{
    uint value;
};
ConstantBuffer<FrameConstantsOffset> frameConstantsOffset : register(b1);

static const FrameConstants frameConstants = BufferLoad < FrameConstants > (frameConstantsOffset.value);

static Texture2D<float2> gBlueNoiseTexture = ResourceDescriptorHeap[frameConstants.blueNoiseBufferSrvId]; 

float2 LoadBlueNoise(uint2 pixelPosition)
{
    return gBlueNoiseTexture.Load(int3( pixelPosition % frameConstants.blueNoiseTextureSize, 0));}
