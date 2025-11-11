#include "Common.hlsli"

#include "GeometryData.hlsli"
#include "PBRMeshInterpolants.hlsli"

struct RootConstants 
{
    uint instanceDataOffset;
    uint reserved1;
    uint vertexBuffersOffset;
    uint vertexCount;
    uint unused4;
    uint unused5;
    uint unused6;
    uint unused7;
    uint renderFeatures;
    uint cameraConstantsOffset; 
};
ConstantBuffer<RootConstants> rootConstants : register(b0);

[RootSignature(universalRS)]
MeshInterpolants main(uint index : SV_VertexID, uint instance : SV_InstanceID)
{
    VertexBufferData vertexBufferData = LoadVertexBufferData(index, rootConstants.vertexBuffersOffset, rootConstants.vertexCount);
    float4 positionWS = float4(vertexBufferData.position, 1.0);
    if (IsValidOffset(rootConstants.instanceDataOffset))
    {
        InstanceData instanceData = BufferLoad < InstanceData > (rootConstants.instanceDataOffset, instance);
        positionWS = mul(positionWS, instanceData.transform);
    #ifndef SHADOW_CASTER
        vertexBufferData.normal = mul(vertexBufferData.normal, (float3x3)instanceData.transform);
    #endif
    }
    
    MeshInterpolants output;
    #ifdef SHADOW_CASTER
    float4x4 lightViewProjection = BufferLoad<float4x4>(rootConstants.cameraConstantsOffset);
    output.position = mul(positionWS, lightViewProjection);
    #else
    CameraConstants cameraConstants = BufferLoad < CameraConstants > (rootConstants.cameraConstantsOffset);
    output.position = mul(positionWS, cameraConstants.viewProjectionMatrix);
    output.normal = vertexBufferData.normal;
    output.uv = vertexBufferData.uv;
    output.positionWS = positionWS.xyz;

    #ifdef DEBUG_PER_PIXEL_FACE_NORMAL
    output.positionWS2 = output.positionWS;
    #endif

    #endif

    return output;
}