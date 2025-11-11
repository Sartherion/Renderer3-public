#include "Common.hlsli"

#include "BlurKernel5.hlsli"
#include "BlurFunctions.hlsli"
#include "FrameConstants.hlsli"
#include "ViewHelpers.hlsli"

struct RootConstants
{
    uint inputTextureSrvId;
    uint outputTextureUavId;
};
ConstantBuffer<RootConstants> rootConstants : register(b0);

[numthreads(SIZE_X, SIZE_Y, 1)]
void main(uint3 threadId : SV_DispatchThreadID)
{
    Texture2D inputTexture = ResourceDescriptorHeap[rootConstants.inputTextureSrvId];
    RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[rootConstants.outputTextureUavId];

    if (frameConstants.settings.pathtracerSettings.denoise)
    {
        CameraConstants cameraConstants = frameConstants.mainCameraData;
        GBuffers gBuffers = LoadGBuffers(frameConstants.gBufferSrvIds);

        const float2 texelSize = frameConstants.mainRenderTargetDimensions.texelSize;

        const float2 screenUv = (threadId.xy + 0.5) * texelSize;
    
        outputTexture[threadId.xy] = BilateralBlur(inputTexture,
            gBuffers,
            cameraConstants.projectionMatrix,
            screenUv,
            texelSize,
            direction,
            frameConstants.settings.pathtracerSettings.denoiseNormalSigma,
            frameConstants.settings.pathtracerSettings.denoiseDepthSigma);
    }
    else
    {
        outputTexture[threadId.xy] = inputTexture[threadId.xy];
    }
}
