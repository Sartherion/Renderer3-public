#include "Common.hlsli"

#include "ViewHelpers.hlsli"

struct SubmeshData
{
    uint materialConstantsOffset;
    uint startIndexLocation;
    uint indexCount;
    float4 boundingSphere;
};
struct RootConstants
{
    uint submeshDataOffset;
    uint uavId;
    uint count;
    uint cameraConstantsOffset;
};

struct DrawIndexedArguments 
{
    uint materialConstantsOffset;
    uint indexCount;
    uint instanceCount;
    uint startIndexLocation;
    int baseVertexLocation;
    uint startInstanceLocation;
};

ConstantBuffer<RootConstants> rootConstants : register(b0);

[numthreads(32, 1, 1)]
void main( uint3 threadId : SV_DispatchThreadID )
{
    if (threadId.x >= rootConstants.count)
    {
        return;
    }

    SubmeshData submeshData = BufferLoad <SubmeshData> (rootConstants.submeshDataOffset, threadId.x);
    RWByteAddressBuffer data = ResourceDescriptorHeap[rootConstants.uavId];
    CameraConstants cameraConstants = BufferLoad < CameraConstants > (rootConstants.cameraConstantsOffset);


    float4 boundingSpherePosition = float4(submeshData.boundingSphere.xyz, 1.0);
    submeshData.boundingSphere.xyz = mul(boundingSpherePosition, cameraConstants.viewMatrix).xyz;
    
    if (IsWithinFrustum(submeshData.boundingSphere,
            cameraConstants.frustumData.planeTopNormalVS,
            cameraConstants.frustumData.planeLeftNormalVS,
            cameraConstants.frustumData.nearZ,
            cameraConstants.frustumData.farZ))
    {
        uint count;
        data.InterlockedAdd(0, 1, count);
        DrawIndexedArguments arguments;
        arguments.baseVertexLocation = 0;
        arguments.indexCount = submeshData.indexCount;
        arguments.instanceCount = 1;
        arguments.startIndexLocation = submeshData.startIndexLocation;
        arguments.startInstanceLocation = 0;
        arguments.materialConstantsOffset = submeshData.materialConstantsOffset;
        data.Store(4 + count * sizeof(DrawIndexedArguments), arguments);
    }
}