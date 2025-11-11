#pragma once
#include "ClusteredShadingCommon.hlsli"

struct RootConstants
{
    uint instanceDataOffset;
    uint materialConstantsOffset;
    uint vertexBuffersOffset;
    uint vertexCount;
    uint lightsBufferOffset;
    uint clusterCountX;
    uint clusterCountY;
    uint cameraConstantsOffset;
};
ConstantBuffer<RootConstants> rootConstants : register(b0);

struct Interpolants
{
    uint renderTargetArrayIndex : SV_RenderTargetArrayIndex;
    nointerpolation float3 positionVS : POSITIONVS;
    float4 position : SV_Position;
};
