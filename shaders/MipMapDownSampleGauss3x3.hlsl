#include "BlurKernel3.hlsli"
#include "BlurFunctions.hlsli"

struct RootConstants
{
    uint srvId;
    uint uavId;
    float2 outputTexelSize;
    uint isHorizontal;
};
ConstantBuffer<RootConstants> rootConstants : register(b0);

[numthreads(8, 8, 1)]
void main( uint3 threadId : SV_DispatchThreadID )
{
    Texture2D input = ResourceDescriptorHeap[rootConstants.srvId];
    RWTexture2D<float4> output = ResourceDescriptorHeap[rootConstants.uavId]; 

    float2 direction = float2(0.0, 1.0);
    if (rootConstants.isHorizontal == 1)
    {
        direction = float2(2.0, 0.0);
    }

    output[threadId.xy] = Blur(input, (threadId.xy + 0.5) * rootConstants.outputTexelSize, direction, 0.5 * rootConstants.outputTexelSize);
}
