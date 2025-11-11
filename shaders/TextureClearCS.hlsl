#include "../SharedDefines.h"

struct RootConstants
{
    uint textureUavId;
    uint4 value;
};
ConstantBuffer<RootConstants> rootConstants: register(b0);

[numthreads(textureClearThreadGroupSizeX, textureClearThreadGroupSizeY, 1)]
void main( uint3 threadId: SV_DispatchThreadID )
{
    RWTexture2D<uint4> texture = ResourceDescriptorHeap[rootConstants.textureUavId];

    texture[threadId.xy] = rootConstants.value;
}
