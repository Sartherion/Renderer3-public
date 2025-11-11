#include "Common.hlsli"

#include "ColorHelpers.hlsli"
#include "MathHelpers.hlsli"

struct RootConstants 
{
    uint outputUavId;
    uint inputSrvId;
    float2 outputTexelsize;
    float4 threshold;
};
ConstantBuffer<RootConstants> rootConstants: register(b0);

//@note: following https://catlikecoding.com/unity/tutorials/advanced-rendering/bloom/
float3 ApplyThreshold(float3 color, float4 threshold)
{
    const float brightness = max(max(color.r, color.g), color.b);
    float soft = brightness + threshold.y;
    soft = clamp(soft, 0.0, threshold.z);
    soft = soft * soft * threshold.w;

    float contribution = max(soft, brightness - threshold.x);
    contribution /= max(brightness, 0.00001);
    color *= contribution;
    return color;
}

[numthreads(8, 8, 1)] 
void main(uint3 threadId : SV_DispatchThreadID)
{
    Texture2D input = ResourceDescriptorHeap[rootConstants.inputSrvId];
    RWTexture2D<float4> output = ResourceDescriptorHeap[rootConstants.outputUavId];

    static const float2 offsets[] = { float2(0.0f, 0.0f), float2(-1.0f, -1.0f), float2(-1.0f, 1.0f), float2(1.0f, -1.0f), float2(1.0f, 1.0f) };

    WeightedSum<float3> color = WeightedSum<float3>::Init();
    for (int i = 0; i < 5; i++)
    {
        const float2 sampleUv = (threadId.xy + 0.5 + offsets[i]) * rootConstants.outputTexelsize;

        float3 sampleColor = input.SampleLevel(samplerPointClamp, sampleUv, 0).xyz;
        sampleColor = ApplyThreshold(sampleColor, rootConstants.threshold);
        const float w = 1.0f / (Luminance(sampleColor) + 1.0f);
        color.AddValue(sampleColor, w);
    }
    output[threadId.xy] = float4(color.GetNormalizedSum(), 1.0);
}