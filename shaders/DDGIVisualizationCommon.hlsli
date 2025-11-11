#pragma once
#include "Common.hlsli"
#include "DDGICommon.hlsli"

struct Interpolants
{
    float3 positionOS : POSITIONOS;
    uint instance : INSTANCE;
    float4 position : SV_Position;
};

struct RootConstants
{
    uint ddgiDataOffset;
    uint unused1;
    uint vertexBuffersOffset;
    uint vertexCount;
    uint unused4;
    uint unused5;
    uint unused6;
    uint unused7;
    uint unused8;
    uint cameraConstantsOffset;
};
ConstantBuffer<RootConstants> rootConstants : register(b0);
