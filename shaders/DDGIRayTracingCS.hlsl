#include "DDGICommon.hlsli"
#include "ShadingHelpers.hlsli"
#include "SamplingHelpers.hlsli"
#include "RaytracingHelpers.hlsli"
#include "Random.hlsli"

struct RootConstants
{
    uint ddgiDataOffset;
    uint accelerationStructureSrvId;
    uint skyBoxSrvId;
    uint instanceGeometryDataOffset;
    uint lightingDataBufferOffset;
};
ConstantBuffer<RootConstants> rootConstants : register(b0);

struct AtlasLookupData 
{
    uint unpaddedTileSize;
    uint2 tileCoordinate; // coordinate in unpadded tile
    uint2 atlasCoordinate;
    float3 texelDirection;
};

AtlasLookupData CalculateAtlasLookupData(uint groupIndex, uint probeIndex, DDGIAtlasData atlasData)
{
    AtlasLookupData result;
    result.unpaddedTileSize = atlasData.tileSize - 2;
    result.tileCoordinate = uint2(groupIndex % result.unpaddedTileSize, groupIndex / result.unpaddedTileSize);
    result.atlasCoordinate = result.tileCoordinate + 1 + GetTileOrigin(probeIndex, atlasData);
    result.texelDirection = OctahedralDecode((result.tileCoordinate + 0.5) / result.unpaddedTileSize);

    return result;
}

float ComputeAlpha(float alphaValue, float weightSum, AtlasLookupData atlasCoordinates)
{
    float result;
    result = weightSum > 0.001 ? alphaValue : 1.0;
    if (any(atlasCoordinates.tileCoordinate > atlasCoordinates.unpaddedTileSize))
    {
        result = 1.0;
    }
    return result;
}

static const uint rayCount = 256; 

groupshared float3 radianceValues[rayCount];
groupshared float depthValues[rayCount];
groupshared float2 uvValues[rayCount];

[numthreads(rayCount, 1, 1)]
void main( uint3 threadId: SV_DispatchThreadID, uint groupIndex : SV_GroupIndex )
{
    CameraConstants cameraConstants = frameConstants.mainCameraData;
    LightingData lightingData = BufferLoad <LightingData> (rootConstants.lightingDataBufferOffset);
    DDGIData ddgiData = BufferLoad < DDGIData > (rootConstants.ddgiDataOffset);
    DDGIProbeGridData gridData = ddgiData.probeGridData;

    RWTexture2D<float2> depthAtlas = ResourceDescriptorHeap[ddgiData.depthAtlasData.uavId];
    RWTexture2D<float3> irradianceAtlas = ResourceDescriptorHeap[ddgiData.irradianceAtlasData.uavId]; 
    RWTexture2D<float3> radianceAtlas = ResourceDescriptorHeap[ddgiData.radianceAtlasData.uavId];
    RaytracingAccelerationStructure accelerationStructure = ResourceDescriptorHeap[rootConstants.accelerationStructureSrvId];
    TextureCube skyBox = ResourceDescriptorHeap[rootConstants.skyBoxSrvId];

    const uint probeIndex = threadId.z;
    const float3 probePosition = GetProbePosition(SpatialProbeIndex(probeIndex, gridData.gridDimensions), gridData);

    SeedRng(probeIndex * uint(frameConstants.frameTimings.frameId));
    const float2 u = float2(RandomFloat(), RandomFloat());
    const float3x3 randomRotation = (float3x3) Rotation(SampleFullSphere(u));

    const float3 direction = mul(FibonacciSpiral(groupIndex, rayCount), randomRotation);

    RayDesc ray;
    ray.Origin = probePosition;
    ray.TMin = 0.0;
    ray.TMax = FLT_MAX;
    ray.Direction = direction;

    RayQuery<RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> query;

    query.TraceRayInline(accelerationStructure, RAY_FLAG_NONE, uint(-1), ray);

    const float maxDepth = length(gridData.probeSpacing);
    float depth = maxDepth;
    
    while (query.Proceed())
    {
        if (query.CandidateType() == CANDIDATE_NON_OPAQUE_TRIANGLE)
        {
            query.CommitNonOpaqueTriangleHit(); //@todo: alpha ignored for now!
        }
    }

    float3 radiance = 0;
    if (query.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
    {
        RaytracingHit rtHit = ExtractCommittedResult(query);
        HitData hitData = LoadHitData(rtHit, rootConstants.instanceGeometryDataOffset, frameConstants.settings.materialSettings);
        MaterialData materialData = hitData.material;
        HitGeometryData interpolatedVertexAttributes = hitData.interpolatedVertexAttributes;
        
        RenderFeatures renderFeatures = RenderFeaturesDefaults();
        renderFeatures.forceLastShadowCascade = true;
        renderFeatures.sampleDiffuseDDGI = frameConstants.settings.ddgiSettings.higherBounceIndirectDiffuseIntensity > 0 ? true : false;
        renderFeatures.sampleSpecularDDGI = frameConstants.settings.ddgiSettings.higherBounceIndirectSpecularIntensity > 0 ? true : false;

        LightingSettings lightingSettings = frameConstants.settings.lightingSettings;
        lightingSettings.iblDiffuseIntensity = frameConstants.settings.ddgiSettings.higherBounceIndirectDiffuseIntensity;
        lightingSettings.iblSpecularIntensity = frameConstants.settings.ddgiSettings.higherBounceIndirectSpecularIntensity;
        lightingSettings.useSpecular = false;

        radiance = LightSurface < false > (-direction,
            interpolatedVertexAttributes.position,
            materialData,
            1.0,
            0.0,
            0.0,
            lightingSettings,
            renderFeatures,
            lightingData);

        depth = min(rtHit.rayHitT, maxDepth);
    }
    else
    {
        radiance = skyBox.SampleLevel(samplerLinearClamp, direction, 0).rgb * frameConstants.settings.lightingSettings.skyBrightness;
    }

    float2 uv = OctahedralEncode(direction);
    uvValues[groupIndex] = uv;
    radianceValues[groupIndex] = radiance;
    depthValues[groupIndex] = depth;

    GroupMemoryBarrierWithGroupSync(); 

    AtlasLookupData depthAtlasCoordinates = CalculateAtlasLookupData(groupIndex, probeIndex, ddgiData.depthAtlasData);
    AtlasLookupData irradianceAtlasCoordinates = CalculateAtlasLookupData(groupIndex, probeIndex, ddgiData.irradianceAtlasData);
    AtlasLookupData radianceAtlasCoordinates = CalculateAtlasLookupData(groupIndex, probeIndex, ddgiData.radianceAtlasData);

    WeightedSum<float2> depthSum = WeightedSum<float2>::Init();
    WeightedSum<float3> irradianceSum = WeightedSum<float3>::Init();
    WeightedSum<float3> radianceSum = WeightedSum<float3>::Init();


    for (int i = 0; i < rayCount; i++) 
    {
        const float3 radiance = radianceValues[i];
        float2 depth = depthValues[i];
        depth.y = depth.x * depth.x;
        const float2 uv = uvValues[i];
        const float3 rayDirection = OctahedralDecode(uv);

        const float irradianceWeight = max(0, dot(irradianceAtlasCoordinates.texelDirection, rayDirection));
        irradianceSum.AddValue(radiance, irradianceWeight);

        const static float depthCosinePower = 5.0;
        const float depthWeight = pow(max(0, dot(depthAtlasCoordinates.texelDirection, rayDirection)), depthCosinePower);
        depthSum.AddValue(depth, depthWeight);

        const float radianceWeight = pow(saturate((dot(rayDirection, radianceAtlasCoordinates.texelDirection) - 0.75) / (1 - 0.75)), 4);
        radianceSum.AddValue(radiance, radianceWeight);
    }

    const float alpha = frameConstants.settings.ddgiSettings.historyBlendWeight;
    const float depthAlpha = ComputeAlpha(alpha, depthSum.weightSum, depthAtlasCoordinates);
    const float irradianceAlpha = ComputeAlpha(alpha, irradianceSum.weightSum, irradianceAtlasCoordinates);
    const float radianceAlpha = ComputeAlpha(alpha, radianceSum.weightSum, radianceAtlasCoordinates);

    const float irradianceNormalization = rayCount / (4 * Pi); //@todo: why not normalize by weightsum?
    irradianceSum.valueSum /= irradianceNormalization; 
    #if 1
    irradianceSum.valueSum = pow(irradianceSum.valueSum, 1.0 / frameConstants.settings.ddgiSettings.irradianceGammaExponent);
    #endif

    const float3 historyIrradiance = irradianceAtlas.Load(irradianceAtlasCoordinates.atlasCoordinate);
    const float3 historyRadiance = radianceAtlas.Load(radianceAtlasCoordinates.atlasCoordinate); 
    const float2 historyDepth = depthAtlas.Load(depthAtlasCoordinates.atlasCoordinate);
    
    depthAtlas[int2(radianceAtlasCoordinates.atlasCoordinate)] = lerp(depthSum.GetNormalizedSum().xy, historyDepth, depthAlpha);
    irradianceAtlas[int2(irradianceAtlasCoordinates.atlasCoordinate)] = lerp(irradianceSum.valueSum, historyIrradiance, irradianceAlpha); 
    radianceAtlas[int2(depthAtlasCoordinates.atlasCoordinate)] = lerp(radianceSum.GetNormalizedSum(), historyRadiance, radianceAlpha);
}
