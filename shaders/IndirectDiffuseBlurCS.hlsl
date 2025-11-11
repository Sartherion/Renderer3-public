#include "IndirectDiffuseCommon.hlsli"

static const BlurSamplePatternRotationSetting samplePatternRotationSetting = BlurSamplePatternRotationSetting::PerPixelBlueNoise;
static const float blurRadius = settings.blurRadius;
static const float blurRadiusDepthScale = 20.0;
static const float historyFixBilateralDepthScale = 250.0;
static const bool limitProjectedRadius = false;
static const bool useAOWeight = false; 
static const bool scaleWithAccumulatedFrames = true;

#include "IndirectDiffuseBlur.hlsli"

