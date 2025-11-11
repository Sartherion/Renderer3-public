#include "ClusteredShadingCommon.hlsli"
#include "ViewHelpers.hlsli"

struct RootConstants
{
    uint shellRTId;
    uint depthSrvId;
    uint headNodesBufferId;
    uint linkedLightListId;
    uint lightType;
    uint clusterCountX;
    uint clusterCountY;
    uint cameraConstantsOffset;
};
ConstantBuffer<RootConstants> rootConstants : register(b0);


[numthreads(8, 8, 1)]
void main( uint3 threadId: SV_DispatchThreadID )
{
    CameraConstants cameraConstants = BufferLoad < CameraConstants > (rootConstants.cameraConstantsOffset);
    const float nearZ = cameraConstants.frustumData.nearZ;
    const float farZ = cameraConstants.frustumData.farZ;
    
    if (threadId.x >= rootConstants.clusterCountX || threadId.y >= rootConstants.clusterCountY)
    {
        return;
    }

    Texture2DArray<float2> shellRT = ResourceDescriptorHeap[rootConstants.shellRTId];
    RWBufferWithCounter linkedLightList = { ResourceDescriptorHeap[rootConstants.linkedLightListId] };
    RWByteAddressBuffer headNodesBuffer = ResourceDescriptorHeap[rootConstants.headNodesBufferId];

    float2 minMaxDepth = shellRT.Load(uint4(threadId, 0)); 
    
    if (minMaxDepth.x == 1.0 && minMaxDepth.y == 1.0)
    {
        return;
    }

    #if 0 
    if (IsValidId(rootConstants.depthSrvId))
    {
        Texture2D<float> depthBuffer = ResourceDescriptorHeap[rootConstants.depthSrvId];
        float sceneDepth = depthBuffer.Load(int3(threadId.xy, 0));
        sceneDepth = NonlinearToLinearDepth(sceneDepth, nearZ, farZ);
        sceneDepth = ZSliceDistributionLinear(NormalizeLinearDepth(sceneDepth, nearZ, farZ), clusterCountZ);
        sceneDepth = (clusterCountZ - 1 - sceneDepth) / 255.0; 
        minMaxDepth.y = min(minMaxDepth.y, sceneDepth); 
    }
    #endif

    //@note: additional test for == 1.0 is needed because if light volume extends beyond view volume triangles will be clipped and not write a value
    uint minDepth = (minMaxDepth.x == 1.0) ? 0 : uint(minMaxDepth.x * 255.0 + 0.5);
    //uint maxDepth = (minMaxDepth.y == 1.0) ? (clusterCountZ - 1) : uint(((clusterCountZ - 1.0) / 255.0 - minMaxDepth.y) * 255.0 + 0.5);
    //@note: I find this more intuitive but is likely the same?
    uint maxDepth = (minMaxDepth.y == 1.0) ? (clusterCountZ - 1) : (clusterCountZ - 1) - uint((minMaxDepth.y) * 255.0 + 0.5);

    uint baseOffset = sizeof(uint) * (threadId.x + rootConstants.clusterCountX * threadId.y);
    uint step = sizeof(uint) * rootConstants.clusterCountX * rootConstants.clusterCountY;

    for (uint i = minDepth; i <= maxDepth; i++)
    {
        uint indexCount = linkedLightList.IncrementCounter();

        uint offset = baseOffset + i * step;
        
        uint previousIndex; 
        headNodesBuffer.InterlockedExchange(offset, indexCount, previousIndex);
        
        LightNode node;
        node.lightType = rootConstants.lightType;
        node.lightId = threadId.z;
        node.next = previousIndex;
        
        linkedLightList.Store(indexCount, node);
    }
}