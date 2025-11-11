#pragma once
#include "Common.hlsli"
#include "FrameConstants.hlsli"

#include "Random.hlsli"
#include "SamplingHelpers.hlsli"
#include "ViewHelpers.hlsli"

static const uint accumulatedFramesMaxCount = 32;
static const IndirectDiffuseSettings settings = frameConstants.settings.indirectDiffuseSettings;

static const float lowestMip = 0.0;
static const uint2 outputSize = frameConstants.mainRenderTargetDimensions.size;
static const float2 outputTexelSize = frameConstants.mainRenderTargetDimensions.texelSize;

enum class BlurSamplePatternRotationSetting
{
    None,
    Random,
    PerPixelRandom,
    PerPixelBlueNoise
};
