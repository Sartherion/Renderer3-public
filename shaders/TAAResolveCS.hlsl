#include "Common.hlsli"

#include "ColorHelpers.hlsli"
#include "FrameConstants.hlsli"
#include "SamplingHelpers.hlsli"
#include "ViewHelpers.hlsli"

static const float historyAlpha = 0.9;

struct RootConstants
{
    uint sourceBufferSrvId;
    uint historyBufferInputSrvId;
    uint historyBufferOutputUavId;
};
ConstantBuffer<RootConstants> rootConstants : register(b0);

[numthreads(8, 8, 1)]
void main( uint3 threadId: SV_DispatchThreadID )
{
    Texture2D sourceBuffer = ResourceDescriptorHeap[rootConstants.sourceBufferSrvId];
    Texture2D<float4> historyBufferInput = ResourceDescriptorHeap[rootConstants.historyBufferInputSrvId]; 
    RWTexture2D<float4> historyBufferOutput = ResourceDescriptorHeap[rootConstants.historyBufferOutputUavId];

    GBuffers gBuffers = LoadGBuffers(frameConstants.gBufferSrvIds);

    //@note: adapted from https://alextardif.com/TAA.html
    WeightedSum<float3> sourceSampleSum = WeightedSum<float3>::Init();
    float3 neighborhoodMin = FLT_MAX;
    float3 neighborhoodMax = -FLT_MAX;
    float3 m1 = 0;
    float3 m2 = 0;
    float closestDepth = 1.0;
    int2 closestDepthPixelPosition = 0;
    
    for (int x = -1; x <= 1; x++)
    {
        for (int y = -1; y <= 1; y++)
        {
            int2 pixelPosition = threadId.xy + int2(x, y);
            pixelPosition = clamp(pixelPosition, 0, frameConstants.mainRenderTargetDimensions.size - 1);
            
            float3 neighbor = max(0, sourceBuffer.Load(int3(pixelPosition, 0)).rgb);
            float subSampleWeight = Filter(length(float2(x, y)), FilterType::Mitchell);

            sourceSampleSum.AddValue(neighbor, subSampleWeight);

            neighborhoodMin = min(neighbor, neighborhoodMin);
            neighborhoodMax = max(neighbor, neighborhoodMax);
            
            m1 += neighbor;
            m2 += neighbor * neighbor;

            const float sampleDepth = gBuffers.depth.Load(int3(pixelPosition, 0));

            if (sampleDepth < closestDepth) //@note: according to https://www.elopezr.com/temporal-aa-and-the-quest-for-the-holy-trail/ this inflates the edges to reduce AA?
            {
                closestDepth = sampleDepth;
                closestDepthPixelPosition = pixelPosition;
            }
        }
    }

    const float3 sourceSample = sourceSampleSum.GetNormalizedSum();
   
    const float2 motionVector = gBuffers.velocities.Load(int3(closestDepthPixelPosition, 0)) * float2(0.5, -0.5);

    const float2 texelSize = frameConstants.mainRenderTargetDimensions.texelSize;
    const float2 sourceUv = (threadId.xy + 0.5) * texelSize; 
    const float2 historyUv = sourceUv - motionVector;
    if (!IsWithinTextureBorders(historyUv))
    {
        historyBufferOutput[threadId.xy] = float4(sourceSample, 1.0);
    }
    else
    {
        float3 historySample = SampleTextureCatmullRom(historyBufferInput, samplerLinearClamp, historyUv, frameConstants.mainRenderTargetDimensions.size).rgb;

        const float normalization = 1.0 / 9.0;
        const float gamma = 1.0f;
        const float3 mu = m1 * normalization;
        const float3 sigma = sqrt(abs((m2 * normalization) - mu * mu));
        const float3 minc = mu - gamma * sigma;
        const float3 maxc = mu + gamma * sigma;
        
        historySample = clamp(historySample, neighborhoodMin, neighborhoodMax);

        historySample = ClipAABB(minc, maxc, historySample);

        float3 compressedSource = sourceSample * rcp(max(max(sourceSample.r, sourceSample.g), sourceSample.b) + 1.0);
        float3 compressedHistory = historySample * rcp(max(max(historySample.r, historySample.g), historySample.b) + 1.0);

        float luminanceSource = Luminance(compressedSource);
        float luminanceHistory = Luminance(compressedHistory);

        float historyWeight = historyAlpha;
        float sourceWeight = 1.0 - historyWeight;
        
        sourceWeight *= 1.0 / (1.0 + luminanceSource);
        historyWeight *= 1.0 / (1.0 + luminanceHistory);

        historyBufferOutput[threadId.xy] = float4((sourceSample * sourceWeight + historySample * historyWeight) / max(sourceWeight + historyWeight, 0.00001), 1.0);
    }
}