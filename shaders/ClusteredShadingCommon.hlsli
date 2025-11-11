#pragma once
#include "Common.hlsli"
#include "../SharedDefines.h"

struct ClusterData
{
    uint lightListHeadNodesId;
    uint linkedLightListId;
    uint3 clusterCount;
};

struct LightNode
{
    uint lightType : 8;
    uint lightId : 24;
    uint next;
};

uint2 CalculateClusterXY(uint2 pixelPosition)
{
    return uint2(pixelPosition.x / clusteredShadingTileSizeX, pixelPosition.y / clusteredShadingTileSizeY); 
}

float NormalizeLinearDepth(float linearDepth, float nearZ, float farZ)
{
    return (linearDepth - nearZ) / (farZ - nearZ);
}

uint ZSliceDistributionLinear(float normalizedDepth, uint clusterCountZ)
{
    return normalizedDepth == 1.0 ? (clusterCountZ - 1) : uint(normalizedDepth * clusterCountZ); 
}

uint3 CalculateClusterIndex(uint2 pixelPosition, float linearDepth, CameraConstants cameraConstants)
{
    uint3 clusterIndex;
    clusterIndex.xy = CalculateClusterXY(pixelPosition);
    clusterIndex.z = ZSliceDistributionLinear(NormalizeLinearDepth(linearDepth, cameraConstants.frustumData.nearZ, cameraConstants.frustumData.farZ), clusterCountZ); 

    return clusterIndex;
}
