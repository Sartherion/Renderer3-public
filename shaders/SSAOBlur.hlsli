#include "Common.hlsli"
#include "BlurKernel5.hlsli"
#include "BlurFunctions.hlsli"

struct RootConstants
{
    uint outputUavId;
    uint inputSrvId;
};
ConstantBuffer<RootConstants> rootConstants : register(b0);

[numthreads(threadGroupSizeX, threadGroupSizeY, 1)]
void main(uint3 threadId: SV_DispatchThreadID)
{
    const float2 texelSize = frameConstants.mainRenderTargetDimensions.texelSize;

    Texture2D inputBuffer = ResourceDescriptorHeap[rootConstants.inputSrvId];
    RWTexture2D<float4> outputBuffer = ResourceDescriptorHeap[rootConstants.outputUavId];

    GBuffers gBuffers = LoadGBuffers(frameConstants.gBufferSrvIds);
    CameraConstants cameraConstants = frameConstants.mainCameraData;

    const float2 screenUv = (threadId.xy + 0.5) * texelSize;
    
    outputBuffer[threadId.xy] = BilateralBlur(inputBuffer,
        gBuffers,
        frameConstants.mainCameraData.projectionMatrix,
        screenUv,
        texelSize,
        direction,
        frameConstants.settings.ssaoSettings.blurNormalSigma,
        frameConstants.settings.ssaoSettings.blurDepthSigma);
}

