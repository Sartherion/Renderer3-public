#include "SkyboxCommon.hlsli"


float4 main(Interpolants input) : SV_TARGET
{
    TextureCubeArray textureCube = ResourceDescriptorHeap[rootConstants.skyboxSrvId];
    return textureCube.Sample(samplerLinearWrap, float4(input.positionOS, 0)) * frameConstants.settings.lightingSettings.skyBrightness;
}