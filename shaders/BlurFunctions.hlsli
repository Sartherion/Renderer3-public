#pragma once
#include "Common.hlsli"
#include "FrameConstants.hlsli"
#include "ViewHelpers.hlsli"

//@note: Whenever this file is included include a BlurKernel header first

// Performs a blur in one direction
template <typename T>
T Blur(Texture2D<T> inputTexture, float2 texCoord, float2 direction, float2 texelSize)
{
    WeightedSum <T> color = WeightedSum<T>::Init();

    for (int i = 0; i < kernelSize; i++)
    {
        const float2 offset = offsets[i] * texelSize * direction;
        color.AddValue(inputTexture.SampleLevel(samplerLinearClamp, texCoord + offset, 0), weights[i]);
    }

    return color.GetNormalizedSum();
}

// Performs a bilateral blur in one direction
template <typename T>
T BilateralBlur(Texture2D<T> inputTexture,
    GBuffers gBuffers,
    float4x4 projectionMatrix,
    float2 uv,
    float2 texelSize,
    float2 direction,
    float normalSigma,
    float depthSigma)
{
    const float3 normal = SampleNormalWS(gBuffers, uv);
    const float linearDepth = NonlinearToLinearDepth(gBuffers.depth.SampleLevel(samplerPointClamp, uv, 0.0).r, projectionMatrix);
    
    WeightedSum<T> color = WeightedSum<T>::Init();
    for (int i = 0; i < kernelSize; i++)
    {
        //@todo: central pixel is loaded again in this loop and unnecessary work is being done
        const float2 offset = offsets[i] * texelSize * direction;
        const float2 sampleUv = uv + offset;
        const float3 normalSample = SampleNormalWS(gBuffers, sampleUv);
        const float linearDepthSample = NonlinearToLinearDepth(gBuffers.depth.SampleLevel(samplerPointClamp, sampleUv, 0.0f).r, projectionMatrix);
        
        const float deltaDepth = abs(linearDepthSample - linearDepth);
        const float depthWeight = exp(-deltaDepth * deltaDepth / (2.0 * depthSigma));
        const float dotNormals = dot(normalSample, normal);
        const float normalWeight = exp(-dotNormals * dotNormals / (2.0 * normalSigma));

        color.AddValue(inputTexture.SampleLevel(samplerLinearClamp, sampleUv, 0), weights[i] * depthWeight * normalWeight);
    }

    return color.GetNormalizedSum();
}
