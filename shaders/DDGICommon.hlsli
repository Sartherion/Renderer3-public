#pragma once
#include "Common.hlsli"
#include "CodationHelpers.hlsli"
#include "FrameConstants.hlsli"
#include "MathHelpers.hlsli"

struct DDGIAtlasData
{
    uint uavId;
    uint srvId;
    uint tileSize;
    uint2 dimensions;
    uint elementsPerRowCount;
};

struct DDGIProbeGridData
{
    uint3 gridDimensions;
    float3 probeSpacing;
    float3 gridCenter;
    float3 gridOrigin;
    uint probeCount;
};

struct DDGIData
{
    DDGIAtlasData depthAtlasData;
    DDGIAtlasData irradianceAtlasData;
    DDGIAtlasData radianceAtlasData;
    DDGIProbeGridData probeGridData;
};

struct GridVertex
{
    uint3 index;
    float weight;
};

static uint3 gridCornerOffsets[] =
{
    uint3(0, 0, 0),
    uint3(1, 0, 0),
    uint3(0, 1, 0),
    uint3(1, 1, 0),
    uint3(0, 0, 1),
    uint3(1, 0, 1),
    uint3(0, 1, 1),
    uint3(1, 1, 1),
};

float f(float x, uint i)
{
    return i == 1 ? x : 1 - x;
}

float TrilinearWeight(float3 fractionalPosition, uint3 cornerOffset)
{
    return f(fractionalPosition.x, cornerOffset.x) * f(fractionalPosition.y, cornerOffset.y) * f(fractionalPosition.z, cornerOffset.z);
    
}

void FindClosestProbes(float3 position, DDGIProbeGridData gridData, out GridVertex cellCorners[8])
{
    float3 normalizedPosition = (position - gridData.gridOrigin) / gridData.probeSpacing;
    uint3 baseIndex = uint3(clamp(normalizedPosition, 0, gridData.gridDimensions));
    float3 fractionalPosition = normalizedPosition - baseIndex;
    for (int i = 0; i < 8; i++)
    {
        cellCorners[i].index = baseIndex + gridCornerOffsets[i];
        cellCorners[i].weight = TrilinearWeight(fractionalPosition, gridCornerOffsets[i]);
    }
}

uint LinearProbeIndex(uint3 probeIndex, uint3 gridDimensions)
{
    return probeIndex.z + probeIndex.y * gridDimensions.z + probeIndex.x * gridDimensions.z * gridDimensions.y;
}

uint3 SpatialProbeIndex(uint linearProbeIndex, uint3 gridDimensions)
{
    return uint3((linearProbeIndex / gridDimensions.z) / gridDimensions.y, (linearProbeIndex / gridDimensions.z) % gridDimensions.y, linearProbeIndex % gridDimensions.z);
}

float3 GetProbePosition(uint3 probeIndex, DDGIProbeGridData gridData)
{
    return gridData.gridOrigin + probeIndex * gridData.probeSpacing;
}

float2 TileUvToPaddedTileUv(float2 uv, float2 paddedTileSize)
{
    float2 paddedTilePixelPosition = 1.0 + uv * (paddedTileSize - 2.0); 
    return paddedTilePixelPosition / paddedTileSize;
}

uint2 GetTileOrigin(uint probeIndex, DDGIAtlasData atlasData)
{
    uint rowIndex = probeIndex / atlasData.elementsPerRowCount;
    uint columnIndex = probeIndex % atlasData.elementsPerRowCount;
    return uint2(columnIndex, rowIndex) * atlasData.tileSize;
}

float2 PaddedTileUvToAtlasUv(float2 uv, uint probeIndex, DDGIAtlasData atlasData)
{
    float2 tilePixelCoordinate = uv * atlasData.tileSize;
    float2 tileOrigin = GetTileOrigin(probeIndex, atlasData);
    float2 atlasPixelCoordinate = tileOrigin + tilePixelCoordinate;
    return atlasPixelCoordinate / atlasData.dimensions;
}

enum class DDGIQuantity
{
    Irradiance,
    Radiance
};

float3 SampleDDGIGrid(float3 position, float3 viewVector, float3 normal, DDGIData ddgiData, DDGIQuantity quantity = DDGIQuantity::Irradiance)
{
    Texture2D<float2> depthAtlas = ResourceDescriptorHeap[ddgiData.depthAtlasData.srvId];

    DDGIAtlasData sampledAtlasData = ddgiData.irradianceAtlasData;
    if (quantity == DDGIQuantity::Radiance)
    {
        sampledAtlasData = ddgiData.radianceAtlasData;
    }
    Texture2D<float3> sampledAtlas = ResourceDescriptorHeap[sampledAtlasData.srvId];
    DDGIProbeGridData gridData = ddgiData.probeGridData;

    //@todo: most of the complications did not improve leaking so may revert them to simpler version
    const float bias = 0.3;
    const float minSpacing = min(min(gridData.probeSpacing.x, gridData.probeSpacing.y), gridData.probeSpacing.z);
    const float3 biasVector = (normal * 0.2 + viewVector * 0.8) * 0.75 * minSpacing * bias;
    const float3 unbiasedPosition = position;
    position += biasVector;

    GridVertex cellCorners[8];
    FindClosestProbes(position, gridData, cellCorners);

    WeightedSum<float3> result = WeightedSum<float3>::Init();
    WeightedSum<float3> fallbackResult = WeightedSum<float3>::Init();
    for (uint i = 0; i < 8; i++)
    {
        const uint3 probeIndex =  cellCorners[i].index;
        const uint linearProbeIndex = LinearProbeIndex(probeIndex, gridData.gridDimensions);

        const float3 probePosition = GetProbePosition(probeIndex, gridData);
        float3 direction = probePosition - position;
        const float3 unbiasedDirection = normalize(probePosition - unbiasedPosition);
        const float distance = length(direction);
        direction /= distance;
       
        const float2 uv = OctahedralEncode(normal); 
        float2 sampledAtlasUv = TileUvToPaddedTileUv(uv, sampledAtlasData.tileSize);
        sampledAtlasUv = PaddedTileUvToAtlasUv(sampledAtlasUv, linearProbeIndex, sampledAtlasData);
        float2 depthUv = TileUvToPaddedTileUv(uv, ddgiData.depthAtlasData.tileSize);
        depthUv = PaddedTileUvToAtlasUv(depthUv, linearProbeIndex, ddgiData.depthAtlasData);

        const float backfaceWeight = Square((dot(unbiasedDirection, normal) + 1) * 0.5) + 0.2;

        //Chebyshev test
        const float2 depth = depthAtlas.SampleLevel(samplerLinearWrap, depthUv, 0);
        const float variance = abs(Square(depth.x) - depth.y);
        float visibilityWeight = variance / (variance + Square(max(0.0, distance - depth.x)));
        visibilityWeight = distance > depth.x ? visibilityWeight : 1.0;
        #if 1 
        visibilityWeight *= Square(visibilityWeight);
        #endif

        float weight = backfaceWeight * visibilityWeight;
        float fallbackWeight = backfaceWeight;

        const float threshold = 0.2;
        if (weight < threshold)
        {
            weight *= Square(weight) / Square(threshold); //@todo: actually seems to lead to higher instability
        }

        if (fallbackWeight < threshold)
        {
            fallbackWeight *= Square(fallbackWeight) / Square(threshold);
        }

        const float trilinearWeight = cellCorners[i].weight + 0.001;
        weight *= trilinearWeight;
        fallbackWeight *= trilinearWeight; 

        const float3 sampledValue = pow(sampledAtlas.SampleLevel(samplerLinearClamp, sampledAtlasUv, 0), frameConstants.settings.ddgiSettings.irradianceGammaExponent * 0.5);

        result.AddValue(sampledValue, weight);
        fallbackResult.AddValue(sampledValue, fallbackWeight);
    }

    return lerp(Square(fallbackResult.GetNormalizedSum()), Square(result.GetNormalizedSum()), saturate(result.weightSum));
}

