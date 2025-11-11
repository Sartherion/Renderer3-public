#pragma once
#include "Common.hlsli"

#include "FrameConstants.hlsli"
#include "Random.hlsli"

struct DebugVisualization
{
    uint renderMode;
    struct RenderMode //@note: This is an emulation of enum class, because enum classes don't work with ByteAddressBuffer templated loads 
    {
        static const uint Default = 0;
        static const uint Albedo = 1;
        static const uint Normals = 2;
        static const uint NormalsOctahedral = 3;
        static const uint IndirectLighting = 4;
        static const uint Ssao = 5;
        static const uint Roughness = 6;
        static const uint Metalness = 7;
        static const uint Velocities = 8;
        static const uint DepthLinear = 9;
        static const uint ClusterLightCount = 10;
        static const uint CascadeIndex = 11;
    };
};

static uint debugCascadeLevel;
void SetDebugCascadeLevel(uint casadeLevel, uint lightIndex)
{
    #ifdef ENABLE_DEBUG_VISUALIZATION
    const uint debugLightIndex = frameConstants.settings.debugVisualizationSettings.debugCascadeIndex;
    if (lightIndex == debugLightIndex)
    {
        debugCascadeLevel = casadeLevel;
    }
    #endif
}

float3 GetDebugCascadeLevelColorHash()
{
    #ifdef ENABLE_DEBUG_VISUALIZATION
    SeedRng(debugCascadeLevel);
    return float3(RandomFloat(), RandomFloat(), RandomFloat());
    #endif
}

static float3 debugIndirectDiffuse;

void SetDebugIndirectDiffuse(float3 indirectDiffuse)
{
    #ifdef ENABLE_DEBUG_VISUALIZATION
    debugIndirectDiffuse = indirectDiffuse;
    #endif
}

float3 GetDebugIndirectDiffuse()
{
    #ifdef ENABLE_DEBUG_VISUALIZATION
    return debugIndirectDiffuse;
    #endif
}

static uint debugClusterCount;

void SetDebugClusterCount(uint clusterCount)
{
    #ifdef ENABLE_DEBUG_VISUALIZATION
    debugClusterCount = clusterCount;
    #endif
}

uint GetDebugClusterCount()
{
    #ifdef ENABLE_DEBUG_VISUALIZATION
    return debugClusterCount;
    #endif
}

