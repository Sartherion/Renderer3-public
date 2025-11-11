#pragma once
#include "UniversalRootSignature.hlsli"
#include "GlobalBuffer.hlsli"
#include "Samplers.hlsli"

//#define DEBUG_PER_PIXEL_FACE_NORMAL 
#define ENABLE_DEBUG_CAMERA
#define ENABLE_DEBUG_VISUALIZATION

#ifdef __INTELLISENSE__
uint ResourceDescriptorHeap[];
#endif

static const uint InvalidId = uint(-1);

bool IsValidId(uint id)
{
    return id != InvalidId;
}

static const uint InvalidOffset = uint(-1);

bool IsValidOffset(uint id)
{
    return id != InvalidOffset;
}

struct TextureDimensions
{
    uint2 size;
    float2 texelSize;
};

struct FrustumData
{
    float3 planeTopNormalVS;
    float3 planeLeftNormalVS;
    float nearZ;
    float farZ;
};

struct CameraConstants
{
    float3 position;
    float4x4 viewProjectionMatrix;
    float4x4 viewMatrix;
    float4x4 projectionMatrix;
    float4x4 inverseViewProjectionMatrix;
    float4x4 inverseViewMatrix;
    float4x4 inverseProjectionMatrix;
    FrustumData frustumData;
    float2 subPixelJitter;
};

