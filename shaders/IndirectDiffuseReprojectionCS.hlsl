#include "IndirectDiffuseCommon.hlsli"

struct RootConstants
{
    uint outputUavId;
    uint inputSrvId;
    uint historyInputSrvId;
    uint frameCountBufferUavId;
    uint previousFrameCountBufferSrvId;
};
ConstantBuffer<RootConstants> rootConstants : register(b0);

float4 TemporalAccumulation(float4 historyAverage, float4 newValue, float accumulatedFramesNewCount);
float4 GatherLinearDepth(Texture2D<float> depthBuffer, float2 gatherUv, float4x4 projectionMatrix);

[numthreads(8, 8, 1)] 
void main(uint3 threadId : SV_DispatchThreadID)
{
    RWTexture2DArray<float4> outputBuffer = ResourceDescriptorHeap[rootConstants.outputUavId];
    Texture2DArray<float4> inputBuffer = ResourceDescriptorHeap[rootConstants.inputSrvId];
    Texture2DArray<float4> historyInputBuffer = ResourceDescriptorHeap[rootConstants.historyInputSrvId];
    RWTexture2D<float> frameCountBuffer = ResourceDescriptorHeap[rootConstants.frameCountBufferUavId];
    Texture2D<float> previousFrameCountBuffer = ResourceDescriptorHeap[rootConstants.previousFrameCountBufferSrvId];

    const CameraConstants cameraConstants = frameConstants.mainCameraData;
    const GBuffers gBuffers = LoadGBuffers(frameConstants.gBufferSrvIds);
    
    const float2 screenUv = float2(threadId.xy + 0.5) * outputTexelSize;
    const float nonlinearDepth = gBuffers.depthPyramide.Load(int3(threadId.xy, lowestMip)).r;
    const float3 positionWS = SSToWS(screenUv, nonlinearDepth, cameraConstants.inverseViewProjectionMatrix);

    const float2 motionVector = gBuffers.velocities[threadId.xy] * float2(0.5, -0.5);
    const float2 reprojectedUv = screenUv - motionVector;

    // Compute disocclusion based on difference between previous view space Z of current sample and values of previous depth buffer at reprojected uvs //@todo: use plane distance?
    BilinearFilter reprojectedBilinearFilter = GetBilinearFilter(saturate(reprojectedUv), outputSize);
    float2 gatherUv = (reprojectedBilinearFilter.origin + 1.0) * outputTexelSize; 
    float4 previousLinearDepthSamples = GatherLinearDepth(gBuffers.previousDepth, gatherUv, frameConstants.previousMainCameraData.projectionMatrix);

    float previousViewZ = mul(float4(positionWS, 1.0), frameConstants.previousMainCameraData.viewMatrix).z;
    float4 deltaZ = abs(previousViewZ - previousLinearDepthSamples) / length(positionWS - cameraConstants.position.xyz);
    float4 occlusion = select(deltaZ > settings.disocclusionDepthThreshold, 0, 1);

    if (!IsWithinTextureBorders(reprojectedUv))
    {
        occlusion = 0.0;
    }

    float4 disocclusionWeights = GetBilinearCustomWeights(reprojectedBilinearFilter, occlusion);

    float4 previousAccumulatedFramesCount = previousFrameCountBuffer.GatherRed(samplerPointClamp, gatherUv).wzxy; 
    previousAccumulatedFramesCount = min(previousAccumulatedFramesCount + 1.0, accumulatedFramesMaxCount);

    float newAccumulatedFramesCount = ApplyBilinearCustomWeights(previousAccumulatedFramesCount.x,
        previousAccumulatedFramesCount.y,
        previousAccumulatedFramesCount.z,
        previousAccumulatedFramesCount.w,
        disocclusionWeights);;

    newAccumulatedFramesCount = newAccumulatedFramesCount > 0 ? newAccumulatedFramesCount : 1;
    newAccumulatedFramesCount = settings.resetHistoryThisFrame ? 1 : newAccumulatedFramesCount;

    float4 historyInputR = SampleWithCustomBilinearWeights(historyInputBuffer, reprojectedBilinearFilter.origin, disocclusionWeights, 0);
    float4 historyInputG = SampleWithCustomBilinearWeights(historyInputBuffer, reprojectedBilinearFilter.origin, disocclusionWeights, 1);
    float4 historyInputB = SampleWithCustomBilinearWeights(historyInputBuffer, reprojectedBilinearFilter.origin, disocclusionWeights, 2);

    float4 newValueR = inputBuffer.Load(int4(threadId.xy, 0, 0));
    float4 newValueG = inputBuffer.Load(int4(threadId.xy, 1, 0));
    float4 newValueB = inputBuffer.Load(int4(threadId.xy, 2, 0));

    outputBuffer[int3(threadId.xy, 0)] = TemporalAccumulation(historyInputR, newValueR, newAccumulatedFramesCount);
    if (settings.denoiseInSHSpace)
    {
        outputBuffer[int3(threadId.xy, 1)] = TemporalAccumulation(historyInputG, newValueG, newAccumulatedFramesCount);
        outputBuffer[int3(threadId.xy, 2)] = TemporalAccumulation(historyInputB, newValueB, newAccumulatedFramesCount);

    }
    frameCountBuffer[threadId.xy] = newAccumulatedFramesCount;
}


float4 TemporalAccumulation(float4 historyAverage, float4 newValue, float accumulatedFramesNewCount)
{
    return (historyAverage * (accumulatedFramesNewCount - 1) + newValue) / accumulatedFramesNewCount;
}

float4 GatherLinearDepth(Texture2D<float> depthBuffer, float2 gatherUv, float4x4 projectionMatrix)
{
    float4 nonlinearDepth = depthBuffer.GatherRed(samplerPointClamp, gatherUv).wzxy;
    float4 linearDepth;
    linearDepth.x = NonlinearToLinearDepth(nonlinearDepth.x, projectionMatrix);
    linearDepth.y = NonlinearToLinearDepth(nonlinearDepth.y, projectionMatrix);
    linearDepth.z = NonlinearToLinearDepth(nonlinearDepth.z, projectionMatrix);
    linearDepth.w = NonlinearToLinearDepth(nonlinearDepth.w, projectionMatrix);

    return linearDepth;
}
