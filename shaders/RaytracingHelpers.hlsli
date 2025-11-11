#pragma once
#include "Common.hlsli"
#include "GeometryData.hlsli"
#include "Material.hlsli"

struct PbrSubmesh
{
    uint materialConstantsOffset;
    uint startIndexLocation;
    uint indexCount;
};

struct RaytracingInstanceGeometryData
{
    uint submeshDataOffset;
	uint indexBufferOffset;
	uint vertexBufferOffset;
	uint vertexCount;
	uint transformOffset; //@todo: this is only needed because CandidiateObjectToWorld3x4() etc. does not seem to work for some reason
};

using IndexFormat = uint;
void LoadIndices(uint baseIndex, uint indexBufferOffset, out uint indices[3])
{
    indices[0] = BufferLoad <IndexFormat> (indexBufferOffset, baseIndex);
    indices[1] = BufferLoad <IndexFormat> (indexBufferOffset, baseIndex + 1);
    indices[2] = BufferLoad <IndexFormat> (indexBufferOffset, baseIndex + 2);
}

template <typename T>
T InterpolateVertexAttribute(uint attributeBufferOffset, uint indices[3], float3 barycentrics)
{
    T attribute[] ={ BufferLoad <T> (attributeBufferOffset, indices[0]), BufferLoad <T> (attributeBufferOffset, indices[1]), BufferLoad <T> (attributeBufferOffset, indices[2]) };

    return barycentrics.x * attribute[0] + barycentrics.y * attribute[1] + barycentrics.z * attribute[2];
}

MaterialData ReadMaterialDataNonUniform(float2 uv, float3 geometryNormal, float3x3 TBN, MaterialConstants materialConstants) 
{
    MaterialData materialData;
    materialData.albedo = materialConstants.albedo;
    materialData.normal = geometryNormal;
    materialData.roughness = materialConstants.roughness;
    materialData.metalness = materialConstants.metalness;
    materialData.specularCubeMapsArrayIndex = materialConstants.specularCubeMapsArrayIndex;

    if (IsValidId(materialConstants.albedoTextureId))
    {
        Texture2D albedoTexture = ResourceDescriptorHeap[NonUniformResourceIndex(materialConstants.albedoTextureId)]; 
        materialData.albedo = albedoTexture.SampleLevel(samplerLinearWrap, uv, 0); 
    }

    if (IsValidId(materialConstants.normalTextureId))
    {
        Texture2D normalTexture = ResourceDescriptorHeap[materialConstants.normalTextureId];
        float3 normalMapSample = normalTexture.SampleLevel(samplerLinearWrap, uv, 0).xyz;
        materialData.normal = PerturbNormal(normalMapSample, TBN);
    }

    if (IsValidId(materialConstants.roughnessTextureId))
    {
        Texture2D roughnessTexture = ResourceDescriptorHeap[NonUniformResourceIndex(materialConstants.roughnessTextureId)];
        materialData.roughness = roughnessTexture.SampleLevel(samplerLinearWrap, uv, 0).r;
    }

    if (IsValidId(materialConstants.metallicTextureId))
    {
        Texture2D metallicTexture = ResourceDescriptorHeap[NonUniformResourceIndex(materialConstants.metallicTextureId)];
        materialData.metalness = metallicTexture.SampleLevel(samplerLinearWrap, uv, 0).r;
    }

    return materialData;
}

struct RaytracingHit // abstracts over differences between committed and candidate hits
{
    uint instanceId; //@note: there is a difference between InstanceIndex and InstanceID
    uint geometryIndex;
    uint primitiveIndex;
    uint firstIndexOffset;
    float3 barycentrics;
    float3 rayOrigin;
    float3 rayDirection;
    float rayHitT;
};

template <RAY_FLAG rayFlags>
RaytracingHit ExtractCandidateResult(RayQuery< rayFlags> query)
{
    RaytracingHit hit;
    hit.instanceId = query.CandidateInstanceID();
    hit.geometryIndex  = query.CandidateGeometryIndex();
    hit.primitiveIndex = query.CandidatePrimitiveIndex();
    hit.firstIndexOffset = hit.primitiveIndex * 3;
    const float2 dxrBarycentrics = query.CandidateTriangleBarycentrics();
    hit.barycentrics = float3(1.0 - dxrBarycentrics.x - dxrBarycentrics.y, dxrBarycentrics.x, dxrBarycentrics.y);
    hit.rayOrigin = query.WorldRayOrigin();
    hit.rayDirection = query.WorldRayDirection();
    hit.rayHitT = query.CandidateTriangleRayT();
    return hit;
}

template <RAY_FLAG rayFlags>
RaytracingHit ExtractCommittedResult(RayQuery< rayFlags> query)
{
    RaytracingHit hit;
    hit.instanceId = query.CommittedInstanceID();
    hit.geometryIndex  = query.CommittedGeometryIndex();
    hit.primitiveIndex = query.CommittedPrimitiveIndex();
    hit.firstIndexOffset = hit.primitiveIndex * 3;
    const float2 dxrBarycentrics = query.CommittedTriangleBarycentrics();
    hit.barycentrics = float3(1.0 - dxrBarycentrics.x - dxrBarycentrics.y, dxrBarycentrics.x, dxrBarycentrics.y);
    hit.rayOrigin = query.WorldRayOrigin();
    hit.rayDirection = query.WorldRayDirection();
    hit.rayHitT = query.CommittedRayT();
    return hit;
}


struct HitGeometryData : VertexBufferData
{
    PbrSubmesh submesh;
    float3 flatNormal; //@todo
};

HitGeometryData LoadAndInterpolateVertexBufferData(RaytracingHit hit, uint instanceGeometryDataOffset)
{
    HitGeometryData result;
    
    RaytracingInstanceGeometryData instanceGeometryData = BufferLoad < RaytracingInstanceGeometryData > (instanceGeometryDataOffset, hit.instanceId);

    VertexBuffersOffsets vertexBuffersOffsets = GetVertexBufferOffsets(instanceGeometryData.vertexBufferOffset, instanceGeometryData.vertexCount);
    result.submesh = BufferLoad < PbrSubmesh > (instanceGeometryData.submeshDataOffset, hit.geometryIndex);

    uint indices[3];
    LoadIndices(result.submesh.startIndexLocation + hit.firstIndexOffset, instanceGeometryData.indexBufferOffset, indices);

    result.uv = InterpolateVertexAttribute <float2> (vertexBuffersOffsets.uv, indices, hit.barycentrics);

    float4x4 instanceTransformMatrix = IdentityMatrix;
    if (IsValidOffset(instanceGeometryData.transformOffset))
    {
        InstanceData instanceData = BufferLoad < InstanceData > (instanceGeometryData.transformOffset);
        instanceTransformMatrix = instanceData.transform;
    }

    result.position = InterpolateVertexAttribute <float3> (vertexBuffersOffsets.position, indices, hit.barycentrics);
    result.position = mul(float4(result.position, 1.0), instanceTransformMatrix).xyz;

    result.normal = normalize(InterpolateVertexAttribute <float3> (vertexBuffersOffsets.normal, indices, hit.barycentrics));
    result.normal = mul(float4(result.normal, 0.0), instanceTransformMatrix).xyz; //@todo: non-uniform scaling
    //    result.flatNormal = cross(positions[1]- positions[0], positions[2] - positions[0]); 
    //result.flatNormal = mul(float4(flatNormal, 0.0), instanceTransformMatrix).xyz;
    return result;
}

struct HitData
{
    HitGeometryData interpolatedVertexAttributes;
    MaterialData material;
};

HitData LoadHitData(RaytracingHit hit, uint instanceGeometryDataOffset, MaterialSettings materialSettings)
{
    HitData result;
    result.interpolatedVertexAttributes = LoadAndInterpolateVertexBufferData(hit, instanceGeometryDataOffset);

    MaterialConstants materialConstants = MaterialConstants::Init();
    if (IsValidOffset(result.interpolatedVertexAttributes.submesh.materialConstantsOffset))
    {
        materialConstants = BufferLoad < MaterialConstants > (result.interpolatedVertexAttributes.submesh.materialConstantsOffset);
    }
    
    ApplyMaterialSettings(materialSettings, materialConstants);

    float3x3 TBN = CalculateCotangentFrame(result.interpolatedVertexAttributes.normal, hit.rayDirection, result.interpolatedVertexAttributes.uv); 
    result.material = ReadMaterialDataNonUniform(result.interpolatedVertexAttributes.uv, result.interpolatedVertexAttributes.normal, TBN, materialConstants);

    return result;
}
