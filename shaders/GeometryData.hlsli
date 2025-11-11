#pragma once
#include "Common.hlsli"

struct InstanceData
{
    float4x4 transform;
    float4x4 inverseTransposeTransform;
};

struct VertexBufferData 
{
    float3 position;
    float3 normal;
    float2 uv;
};

struct VertexBuffersOffsets
{
    uint position;
    uint normal;
    uint uv;
};

VertexBuffersOffsets GetVertexBufferOffsets(uint baseOffset, uint vertexCount)
{
    VertexBuffersOffsets offsets;
    offsets.position = baseOffset;
    offsets.normal = offsets.position + sizeof(float3) * vertexCount;
    offsets.uv = offsets.normal + sizeof(float3) * vertexCount;
    
    return offsets;
}

VertexBufferData LoadVertexBufferData(uint index, uint baseOffset, uint vertexCount)
{
    VertexBufferData result;
    VertexBuffersOffsets offsets = GetVertexBufferOffsets(baseOffset, vertexCount);

    result.position = BufferLoad<float3>(offsets.position, index);
    result.normal = BufferLoad<float3>(offsets.normal, index);
    result.uv = BufferLoad<float2>(offsets.uv, index);

    return result;
}

