#include "IndirectDiffuseCommon.hlsli"

static const BlurSamplePatternRotationSetting samplePatternRotationSetting = BlurSamplePatternRotationSetting::Random;
static const float blurRadius = settings.preBlurRadius;
static const float blurRadiusDepthScale = 20.0f;
static const float historyFixBilateralDepthScale = settings.historyFixBilateralDepthScale; 
static const bool limitProjectedRadius = true;
static const bool useAOWeight = false; 
static const bool scaleWithAccumulatedFrames = false;

#include "IndirectDiffuseBlur.hlsli"
