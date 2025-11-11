#include "IndirectDiffuseCommon.hlsli"
#include "RaytracingHelpers.hlsli"
#include "ShadingHelpers.hlsli"

struct RootConstants
{
    uint outputUavId;
    uint accelerationStructureSrvId;
    uint skyBoxSrvId;
    uint lightingDataBufferOffset;
    uint instanceGeometryDataOffset;
};
ConstantBuffer<RootConstants> rootConstants : register(b0);


[numthreads(8, 8, 1)] 
void main(uint3 threadId : SV_DispatchThreadID)
{
    MaterialSettings materialSettings = frameConstants.settings.materialSettings;
    RaytracingAccelerationStructure accelerationStructure = ResourceDescriptorHeap[rootConstants.accelerationStructureSrvId];
    TextureCube skyBox = ResourceDescriptorHeap[rootConstants.skyBoxSrvId];
    RWTexture2DArray<float4> output = ResourceDescriptorHeap[rootConstants.outputUavId];
    CameraConstants cameraConstants = frameConstants.mainCameraData;
    LightingData lightingData = BufferLoad < LightingData > (rootConstants.lightingDataBufferOffset);

    const GBuffers gBuffers = LoadGBuffers(frameConstants.gBufferSrvIds);
    
    const float2 screenUv = float2(threadId.xy + 0.5) * outputTexelSize;
    const float nonlinearDepth = gBuffers.depthPyramide.Load(int3(threadId.xy, lowestMip)).r;
    const float3 positionWS = SSToWS(screenUv, nonlinearDepth, cameraConstants.inverseViewProjectionMatrix);
    const float3 normalWS = LoadNormalWS(gBuffers, threadId.xy); 

    float2 randomVector = LoadBlueNoise(threadId.xy);

    #if 0 //use interleaved gradient noise instead
    SeedRng(frameConstants.frameTimings.frameId);
    randomVector.x = InterleavedGradientNoise(threadId.xy + rand_pcg() % 1000); 
    randomVector.y = InterleavedGradientNoise(threadId.xy + rand_pcg() % 1000); 
    #endif

    #if 0  //use white noise instead
    uint seed = initRNG(threadId.xy, outputSize, frameConstants.frameTimings.frameId);
    randomVector.x = rand(seed);
    randomVector.y = rand(seed);
    #endif
    
    #if 0 //apply bias
    float bias = 0.9;
    randomVector.x *= (1 - bias);
    #endif

    float pdf;
    float3 directionWS = SampleCosineWeightedHemisphere(randomVector, pdf);
    float3x3 TBNMatrix = (float3x3) Rotation(normalWS);
    directionWS = mul(directionWS, TBNMatrix);

    //@todo: trace in screen space first
    RayDesc ray;
    ray.Origin = positionWS /*+ normalWS* 0.01*/; //@todo: use proper offset
    ray.TMin = 0.01; //@note: choosing this to large will remove contact shadows
    ray.TMax = FLT_MAX;
    ray.Direction = directionWS;

    RayQuery < RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES > query;
    query.TraceRayInline(accelerationStructure, RAY_FLAG_NONE, uint(-1), ray);
    
    while (query.Proceed())
    {
        if (query.CandidateType() == CANDIDATE_NON_OPAQUE_TRIANGLE)
        {
            #if 0 //ignore alpha
            query.CommitNonOpaqueTriangleHit(); 
            #else
            RaytracingHit rtHit = ExtractCandidateResult(query);
            HitData hitData = LoadHitData(rtHit, rootConstants.instanceGeometryDataOffset, materialSettings);
            if (hitData.material.albedo.a > 0.05)
            {
                query.CommitNonOpaqueTriangleHit();
            }
            #endif
        }
    }

    float3 radiance = 0.0;
    float ambientOcclusion = 1.0;
    if (query.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
    {
        RaytracingHit rtHit = ExtractCommittedResult(query);
        HitData hitData = LoadHitData(rtHit, rootConstants.instanceGeometryDataOffset, materialSettings);

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

        radiance = LightSurface < false > (-directionWS, interpolatedVertexAttributes.position, materialData, 1.0, 0.0, 0.0, lightingSettings, renderFeatures, lightingData);
        ambientOcclusion = rtHit.rayHitT > settings.rtaoHitDistanceThreshold;
    }
    else
    {
        radiance = skyBox.SampleLevel(samplerLinearClamp, directionWS, 0).rgb * frameConstants.settings.lightingSettings.skyBrightness;
    }
    
    if (settings.denoiseInSHSpace)
    {
        radiance /= pdf; //@todo: is this correct?
        radiance = clamp(radiance, 0, FLT_MAX);
        output[int3(threadId.xy, 0)] = EvaluateSH4Basis(directionWS) * radiance.r;
        output[int3(threadId.xy, 1)] = EvaluateSH4Basis(directionWS) * radiance.g;
        output[int3(threadId.xy, 2)] = EvaluateSH4Basis(directionWS) * radiance.b;
        //@todo: where store rtao in this case?
    }
    else
    {
        radiance *= dot(directionWS, normalWS) / pdf;
        radiance = clamp(radiance, 0, FLT_MAX);
        output[int3(threadId.xy, 0)] = float4(radiance, ambientOcclusion);
    }
}


