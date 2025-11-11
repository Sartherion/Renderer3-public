#include "Common.hlsli"

#include "DebugVisualization.hlsli"

struct RootConstants 
{
    uint highResolutionOutputUavId;
    uint highResolutionInputSrvId;
    uint lowResolutionInputSrvId;
    float intensity;
    float2 outputTexelSize;
};
ConstantBuffer<RootConstants> rootConstants: register(b0); 

[numthreads(8, 8, 1)] 
void main(uint3 threadId : SV_DispatchThreadID)
{
    RWTexture2D<float4> highResolutionOutputBuffer = ResourceDescriptorHeap[rootConstants.highResolutionOutputUavId];
    Texture2D highResolutionInput = ResourceDescriptorHeap[rootConstants.highResolutionInputSrvId];
    Texture2D lowResolutionInput = ResourceDescriptorHeap[rootConstants.lowResolutionInputSrvId];
   
    float3 color = highResolutionInput.Load(int3(threadId.xy, 0)).rgb;

    const float2 screenUv = (threadId.xy + 0.5) * rootConstants.outputTexelSize;

    if (frameConstants.settings.debugVisualizationSettings.renderMode == DebugVisualization::RenderMode::Default) 
    {
        color += rootConstants.intensity * lowResolutionInput.SampleLevel(samplerLinearClamp, screenUv, 0).rgb;
    }

    highResolutionOutputBuffer[threadId.xy] = float4(color, 1.0f);
}