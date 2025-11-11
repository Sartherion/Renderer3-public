#include "IndirectDiffuseCommon.hlsli"

static const float historyFixWeightSumThreshold = 3.0;
static const uint historyFixFrameMaxCount = 4;

struct RootConstants
{
    //@note: input and output buffers are assumed to be the same. Separate srv is only needed because uav cannot read from multiple mips.
    uint outputUavId;
    uint inputSrvId;
    uint accumulatedFramesCountBufferSrvId;
};
ConstantBuffer<RootConstants> rootConstants : register(b0);

float4 GatherLinearDepthMip(GBuffers gBuffers, int2 sampleOrigin, int mipLevel, float4x4 projectionMatrix);

[numthreads(8, 8, 1)] 
void main(uint3 threadId : SV_DispatchThreadID)
{
    #if 0 // skip history fix
    return;
    #endif

    RWTexture2DArray<float4> outputBuffer = ResourceDescriptorHeap[rootConstants.outputUavId];
    Texture2DArray<float4> inputBuffer = ResourceDescriptorHeap[rootConstants.inputSrvId];
    Texture2D<float> accumulatedFramesCountBuffer = ResourceDescriptorHeap[rootConstants.accumulatedFramesCountBufferSrvId];

    CameraConstants cameraConstants = frameConstants.mainCameraData;
    const GBuffers gBuffers = LoadGBuffers(frameConstants.gBufferSrvIds);

    const float accumulatedFramesCount = accumulatedFramesCountBuffer.Load(int3(threadId.xy, 0));
    const uint framesSinceHistoryReset = accumulatedFramesCount - 1;

    if (framesSinceHistoryReset > historyFixFrameMaxCount) 
    {
        return;
    }

    const int startMipLevel = historyFixFrameMaxCount - framesSinceHistoryReset;

    const float nonlinearDepth = gBuffers.depthPyramide.Load(int3(threadId.xy, lowestMip)).r;
    const float linearDepth = NonlinearToLinearDepth(nonlinearDepth, cameraConstants.projectionMatrix);
    const float2 screenUv = float2(threadId.xy + 0.5) * outputTexelSize;

    uint2 sampleOrigin = 0;
    float4 bilateralWeights = 0;
    int mipLevel = startMipLevel;

    while( mipLevel > 0 )
    {
        uint2 mipSize = float2(outputSize) / (1u << mipLevel);
        float2 mipTexelSize = 1.0 / mipSize;

        sampleOrigin = GetSampleOrigin(screenUv, mipSize);
        const float4 coarseDepthSamples = GatherLinearDepthMip(gBuffers, sampleOrigin, mipLevel, cameraConstants.projectionMatrix);

        bilateralWeights = exp(-abs(coarseDepthSamples - linearDepth) * settings.historyFixBilateralDepthScale);

        if (ComponentSum(bilateralWeights) < historyFixWeightSumThreshold)
        {
            mipLevel--;
        }
        else
        {
            break;
        }
    }

    // depth mismatch prevented usage of a blurred sample, so just use original
    if (mipLevel == 0)
    {
        return;
    }

    bilateralWeights = Square(bilateralWeights);

    #if 0 // use a single load from lower mip instead of all the bilateral stuff. Does not actually look that much worse.
    float4 outputValueR = inputBuffer.Load(int4(sampleOrigin, 0, startMipLevel));
    float4 outputValueG = inputBuffer.Load(int4(sampleOrigin, 1, startMipLevel));
    float4 outputValueB = inputBuffer.Load(int4(sampleOrigin, 2, startMipLevel));
    #else
    float4 outputValueR = SampleWithCustomBilinearWeights(inputBuffer, sampleOrigin, bilateralWeights, 0, mipLevel);
    float4 outputValueG = SampleWithCustomBilinearWeights(inputBuffer, sampleOrigin, bilateralWeights, 1, mipLevel);
    float4 outputValueB = SampleWithCustomBilinearWeights(inputBuffer, sampleOrigin, bilateralWeights, 2, mipLevel);
    #endif

    outputBuffer[int3(threadId.xy, 0)] = outputValueR;
    if (settings.denoiseInSHSpace)
    {
        outputBuffer[int3(threadId.xy, 1)] = outputValueG;
        outputBuffer[int3(threadId.xy, 2)] = outputValueB;
    }
}

float4 GatherLinearDepthMip(GBuffers gBuffers, int2 sampleOrigin, int mipLevel, float4x4 projectionMatrix)
{
    //@note: cannot actually use gather, since a mip version is not available
    float4 nonlinearDepth;
    nonlinearDepth.x = gBuffers.depthPyramide.Load(int3(sampleOrigin + int2(0, 0), mipLevel)).r;
    nonlinearDepth.y = gBuffers.depthPyramide.Load(int3(sampleOrigin + int2(0, 1), mipLevel)).r;
    nonlinearDepth.z = gBuffers.depthPyramide.Load(int3(sampleOrigin + int2(1, 0), mipLevel)).r;
    nonlinearDepth.w = gBuffers.depthPyramide.Load(int3(sampleOrigin + int2(1, 1), mipLevel)).r;

    float4 linearDepth;
    linearDepth.x = NonlinearToLinearDepth(nonlinearDepth.x, projectionMatrix);
    linearDepth.y = NonlinearToLinearDepth(nonlinearDepth.y, projectionMatrix);
    linearDepth.z = NonlinearToLinearDepth(nonlinearDepth.z, projectionMatrix);
    linearDepth.w = NonlinearToLinearDepth(nonlinearDepth.w, projectionMatrix);

    return linearDepth;
}
