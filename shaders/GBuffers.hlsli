#pragma once
#include "CodationHelpers.hlsli"
#include "Samplers.hlsli"

struct GBufferSrvIds
{
    uint albedo;
    uint previousAlbedo;
    uint normals;
    uint previousNormals;
    uint velocities;
    uint depth;
    uint previousDepth;
    uint depthPyramide;
};

struct GBuffers
{
    Texture2D albedo;
    Texture2D previousAlbedo;
    Texture2D normals;
    Texture2D previousNormals;
    Texture2D<float2> velocities;
    Texture2D<float> depth;
    Texture2D<float> previousDepth;
    Texture2D<float2> depthPyramide;
};

GBuffers LoadGBuffers(GBufferSrvIds srvIds)
{
    GBuffers gBuffers;
    gBuffers.albedo = ResourceDescriptorHeap[srvIds.albedo];
    gBuffers.previousAlbedo = ResourceDescriptorHeap[srvIds.previousAlbedo];
    gBuffers.normals = ResourceDescriptorHeap[srvIds.normals];
    gBuffers.previousNormals = ResourceDescriptorHeap[srvIds.previousNormals];
    gBuffers.velocities= ResourceDescriptorHeap[srvIds.velocities];
    gBuffers.depth = ResourceDescriptorHeap[srvIds.depth];
    gBuffers.previousDepth = ResourceDescriptorHeap[srvIds.previousDepth];
    gBuffers.depthPyramide = ResourceDescriptorHeap[srvIds.depthPyramide];
    return gBuffers;
}

float3 LoadNormalWS(GBuffers gBuffers, int2 location)
{
    return OctahedralDecode(gBuffers.normals.Load(int3(location, 0)).xy);
}

float LoadRoughness(GBuffers gBuffers, int2 location)
{
    return gBuffers.normals.Load(int3(location, 0)).a;
}

float LoadMetalness(GBuffers gBuffers, int2 location)
{
    return gBuffers.normals.Load(int3(location, 0)).b;
}

float3 SampleNormalWS(GBuffers gBuffers, float2 uv, int2 offset = 0)
{
    return OctahedralDecode(gBuffers.normals.SampleLevel(samplerLinearClamp, uv, 0, offset).xy);
}

float SampleRoughness(GBuffers gBuffers, float2 uv, int2 offset = 0)
{
    return gBuffers.normals.SampleLevel(samplerLinearClamp, uv, 0, offset).a;
}

float SampleMetalness(GBuffers gBuffers, float2 uv, int2 offset = 0)
{
    return gBuffers.normals.SampleLevel(samplerLinearClamp, uv, 0, offset).b;
}

float3 SamplePreviousNormalWS(GBuffers gBuffers, float2 uv, int2 offset = 0)
{
    return OctahedralDecode(gBuffers.previousNormals.SampleLevel(samplerLinearClamp, uv, 0, offset).xy);
}
