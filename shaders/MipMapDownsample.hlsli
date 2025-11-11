#include "Common.hlsli"

enum class SampleMode : uint
{
    Bilinear,
    Minimum,
    Maximum,
    MinimumMaximum
};

float4 Sample(Texture2D texture, float2 uv, SampleMode sampleMode);
float4 InterpolateSamples(float4 sample1, float4 sample2, SampleMode sampleMode);

struct RootConstants
{
    uint srvId;
    uint uavId;
    float2 outputTexelSize;
    uint2 inputDimensions;
};
ConstantBuffer<RootConstants> rootConstants : register(b0);

[numthreads(8, 8, 1)]
void main( uint3 threadId : SV_DispatchThreadID )
{
    SampleMode sampleMode = SAMPLE_MODE;

    Texture2D inputTexture = ResourceDescriptorHeap[rootConstants.srvId];
    RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[rootConstants.uavId]; 

    bool widthIsEven = ((rootConstants.inputDimensions.x) & 1) == 0; //@note: sampling frequency is determined by input dimensions not output
    bool heightIsEven = ((rootConstants.inputDimensions.y) & 1) == 0;

    if (widthIsEven)
    {
        if (heightIsEven)
        {
            float2 uv = rootConstants.outputTexelSize * (threadId.xy + 0.5f);
            outputTexture[threadId.xy] = Sample(inputTexture, uv, sampleMode);
        }
        else
        {
            float2 uv1 = rootConstants.outputTexelSize * (threadId.xy + float2(0.5, 0.25));
            float2 uv2 = rootConstants.outputTexelSize * (threadId.xy + float2(0.5, 0.75));
            float4 sample1 = Sample(inputTexture, uv1, sampleMode);
            float4 sample2 = Sample(inputTexture, uv2, sampleMode);
            outputTexture[threadId.xy] = InterpolateSamples(sample1, sample2, sampleMode);
        }
    }
    else
    {
        if (heightIsEven)
        {
            float2 uv1 = rootConstants.outputTexelSize * (threadId.xy + float2(0.25, 0.5));
            float2 uv2 = rootConstants.outputTexelSize * (threadId.xy + float2(0.75, 0.5));
            float4 sample1 = Sample(inputTexture, uv1, sampleMode);
            float4 sample2 = Sample(inputTexture, uv2, sampleMode);
            outputTexture[threadId.xy] = InterpolateSamples(sample1, sample2, sampleMode);
        }
        else
        {
            float2 uv1 = rootConstants.outputTexelSize * (threadId.xy + float2(0.25, 0.25));
            float2 uv2 = rootConstants.outputTexelSize * (threadId.xy + float2(0.75, 0.25));
            float2 uv3 = rootConstants.outputTexelSize * (threadId.xy + float2(0.25, 0.75));
            float2 uv4 = rootConstants.outputTexelSize * (threadId.xy + float2(0.75, 0.75));
            float4 sample1 = Sample(inputTexture, uv1, sampleMode);
            float4 sample2 = Sample(inputTexture, uv2, sampleMode);
            float4 sample3 = Sample(inputTexture, uv3, sampleMode);
            float4 sample4 = Sample(inputTexture, uv4, sampleMode);
            float4 sample12 = InterpolateSamples(sample1, sample2, sampleMode);
            float4 sample34 = InterpolateSamples(sample3, sample4, sampleMode);
            outputTexture[threadId.xy] = InterpolateSamples(sample12, sample34, sampleMode);
        }
    }
}

float4 Sample(Texture2D texture, float2 uv, SampleMode sampleMode)
{
    switch (sampleMode)
    {
        case SampleMode::Bilinear:
           return texture.SampleLevel(samplerLinearClamp, uv, 0);
        case SampleMode::Minimum:
        {
            return texture.SampleLevel(samplerMinimum, uv, 0);
        }
        case SampleMode::Maximum:
        {
            return texture.SampleLevel(samplerMaximum, uv, 0);
        }
        case SampleMode::MinimumMaximum:
        {
            float minimum = texture.SampleLevel(samplerMinimum, uv, 0).r;
            float maximum = texture.SampleLevel(samplerMaximum, uv, 0).g;
            return float4(minimum, maximum, 0.0, 0.0);  
        }
    }
}

float4 InterpolateSamples(float4 sample1, float4 sample2, SampleMode sampleMode)
{
    switch (sampleMode)
    {
        case SampleMode::Bilinear:
            return 0.5 * (sample1 + sample2);
        case SampleMode::Minimum:
            return min(sample1, sample2);
        case SampleMode::Maximum:
            return max(sample1, sample2);
        case SampleMode::MinimumMaximum:
        {
           float minimum = min(sample1.r, sample2.r);
           float maximum = max(sample1.g, sample2.g);
           return float4(minimum, maximum, 0, 0);
        }
    }
}
