#pragma once
#include "Common.hlsli"

#include "FrameConstants.hlsli"

struct Interpolants
{
    float3 positionOS : POSITIONOS;
    float4 position : SV_Position;
};

struct RootConstants
{
    uint skyboxSrvId;
    uint unused1;
    uint vertexBuffersOffset;
    uint unused3;
    uint unused4;
    uint unused5;
    uint unused6;
    uint unused7;
    uint unused8;
    uint cameraConstantsOffset;

};
ConstantBuffer<RootConstants> rootConstants : register(b0);
