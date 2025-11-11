#include "../SharedDefines.h"

struct RootConstants
{
    uint bufferUavId;
    uint value;
    uint elementsPerThreadCount;
    uint dispatchSize; 
};
ConstantBuffer<RootConstants> rootConstants: register(b0);

[numthreads(bufferClearThreadGroupSize, 1, 1)]
void main( uint3 threadId: SV_DispatchThreadID )
{
    RWByteAddressBuffer buffer = ResourceDescriptorHeap[rootConstants.bufferUavId];

    for (uint i = 0; i < rootConstants.elementsPerThreadCount; i++)
    {
        buffer.Store((threadId.x + i * bufferClearThreadGroupSize * rootConstants.dispatchSize) * sizeof(uint), rootConstants.value);
    }
}