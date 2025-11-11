#pragma once
#include "IndirectDiffuseCommon.hlsli"

//@note: c.f. https://github.com/bartwronski/PoissonSamplingGenerator
static const uint poissonDiskSampleCount = 8;
static const float2 poissonDiskSamples[poissonDiskSampleCount] =
{
    float2(0.0f, 0.0f),
    float2(-0.9046262229702641f, -0.40541152902098343f),
    float2(0.5757397601520097f, 0.8105562089302681f),
    float2(-0.6900069991323438f, 0.3038345193970897f),
    float2(0.639578502605772f, -0.6630578549430495f),
    float2(-0.2639231608669215f, 0.8653144501404262f),
    float2(-0.3128822794631241f, -0.7850121422100734f),
    float2(0.7053459206435717f, 0.05245127051848424f),
};

struct RootConstants
{
    uint outputUavId;
    uint inputSrvId;
    uint accumulationBufferSrvId;
    uint ssaoBufferSrvId;
};
ConstantBuffer<RootConstants> rootConstants : register(b0);

[numthreads(8, 8, 1)] 
void main(uint3 threadId : SV_DispatchThreadID)
{
    RWTexture2DArray<float4> output = ResourceDescriptorHeap[rootConstants.outputUavId];
    Texture2DArray<float4> input = ResourceDescriptorHeap[rootConstants.inputSrvId];
    Texture2D<float4> ssaoBuffer = ResourceDescriptorHeap[rootConstants.ssaoBufferSrvId];
    CameraConstants cameraConstants = frameConstants.mainCameraData;

    const GBuffers gBuffers = LoadGBuffers(frameConstants.gBufferSrvIds);

    const float2 screenUv = float2(threadId.xy + 0.5) * outputTexelSize;
    const float nonlinearDepth = gBuffers.depthPyramide.Load(int3(threadId.xy, lowestMip)).r;
    const float3 positionWS = SSToWS(screenUv, nonlinearDepth, cameraConstants.inverseViewProjectionMatrix);
    const float linearDepth = NonlinearToLinearDepth(nonlinearDepth, frameConstants.mainCameraData.projectionMatrix);

    const float3 normalWS = LoadNormalWS(gBuffers, threadId.xy); 
    const float3x3 TBNMatrix = (float3x3) Rotation(normalWS);

    float theta = 0;

    switch (samplePatternRotationSetting)
    {
        case BlurSamplePatternRotationSetting::Random:
            SeedRng(uint(frameConstants.frameTimings.frameId));
            theta = RandomFloat() * 2 * PI;
            break;
        case BlurSamplePatternRotationSetting::PerPixelRandom:
        {
                uint seed = initRNG(threadId.xy, outputSize, uint(frameConstants.frameTimings.frameId));
                theta = rand(seed) * 2 * PI;
                break;
        }
        case BlurSamplePatternRotationSetting::PerPixelBlueNoise:
            theta = LoadBlueNoise(threadId.xy).r;
            break;
    }
    
    const float cosTheta = cos(theta);
    const float sinTheta = sin(theta);

    float accumulatedFrames = 1.0;
    if (scaleWithAccumulatedFrames)
    {
        Texture2D<float> accumulationBuffer = ResourceDescriptorHeap[rootConstants.accumulationBufferSrvId];
        accumulatedFrames = accumulationBuffer.Load(int3(threadId.xy, 0));
    }

    float4 irradianceR = 0; 
    float4 irradianceG = 0; 
    float4 irradianceB = 0; 
    float weightSum = 0;

    for (int i = 0; i < poissonDiskSampleCount; i++)
    {
        float2 poissonSample = poissonDiskSamples[i];
        poissonSample.x = poissonSample.x * cosTheta - poissonSample.y * sinTheta;
        poissonSample.y = poissonSample.x * sinTheta + poissonSample.y * cosTheta;

        const float radius = saturate(linearDepth / blurRadiusDepthScale) * blurRadius / accumulatedFrames;

        float3 poissonSampleWS = positionWS + mul(radius * float3(poissonSample, 0), TBNMatrix);
        float3 poissonSampleUv = WSToSS(poissonSampleWS, cameraConstants.viewProjectionMatrix);

        float poissonSampleNonlinearDepth = gBuffers.depthPyramide.SampleLevel(samplerPointClamp, poissonSampleUv.xy, 0).r;

        float3 poissonSampleNormalWS = SampleNormalWS(gBuffers, poissonSampleUv.xy);

        float weight = pow(saturate(dot(normalWS, poissonSampleNormalWS)), settings.normalWeightExponent);


        poissonSampleWS = SSToWS(poissonSampleUv.xy, poissonSampleNonlinearDepth, cameraConstants.inverseViewProjectionMatrix);

        float planeDistNorm = accumulatedFrames * historyFixBilateralDepthScale / (1.0 + linearDepth);
        float3 sampleVector = poissonSampleWS - positionWS;
        float geometryWeight = GetGeometryWeight(sampleVector, normalWS, planeDistNorm);
        weight *= geometryWeight;

        if (limitProjectedRadius) //@note: produces artifacts when used for recurrent blurs
        {
            weight *= LengthSquared(sampleVector) <= Square(radius) ? 1 : 1e-4;
        }

        if (useAOWeight)
        {
            float ssaoWeight = ssaoBuffer.SampleLevel(samplerLinearClamp, poissonSampleUv.xy, 0).a;
            weight *= ssaoWeight;
        }

        irradianceR += weight * input.SampleLevel(samplerPointClamp, float3(poissonSampleUv.xy, 0), 0); //@todo: name: actually only irradiance in non-sh case
        irradianceG += weight * input.SampleLevel(samplerPointClamp, float3(poissonSampleUv.xy, 1), 0);
        irradianceB += weight * input.SampleLevel(samplerPointClamp, float3(poissonSampleUv.xy, 2), 0);
        weightSum += weight;
    }

    float normalization = max(1e-4, weightSum);
    output[int3(threadId.xy, 0)] = irradianceR / normalization;
    if (settings.denoiseInSHSpace)
    {
        output[int3(threadId.xy, 1)] = irradianceG / normalization;
        output[int3(threadId.xy, 2)] = irradianceB / normalization;
    }
}
