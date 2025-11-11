#include "SSSRCommon.hlsli"
#include "Random.hlsli"

#ifdef USE_HALF_RESOLUTION_RAY_HIT_BUFFER
static const float lowestMip = 1.0;
#else
static const float lowestMip = 0.0;
#endif
static const float maxRayMarchingMip = 11; 

struct RootConstants
{
    uint outputUavId;
};
ConstantBuffer<RootConstants> rootConstants : register(b0);

[numthreads(8, 8, 1)] 
void main(uint3 threadId : SV_DispatchThreadID)
{
    RWTexture2D<float4> output = ResourceDescriptorHeap[rootConstants.outputUavId];

    Texture2D<float> blueNoise = ResourceDescriptorHeap[frameConstants.blueNoiseBufferSrvId];
    CameraConstants cameraConstants = frameConstants.mainCameraData;

    const GBuffers gBuffers = LoadGBuffers(frameConstants.gBufferSrvIds);
    
    const float2 screenUv = float2(threadId.xy + 0.5) * rayHitBufferDimensions.texelSize;
    const float nonlinearDepth = gBuffers.depthPyramide.Load(int3(threadId.xy, lowestMip)).r;
    const float3 originWS = SSToWS(screenUv, nonlinearDepth, cameraConstants.inverseViewProjectionMatrix);
    
    const float3 viewVectorWS = normalize(cameraConstants.position.xyz - originWS);

    const float3 normalWS = SampleNormalWS(gBuffers, screenUv);
    const float roughness = max(SampleRoughness(gBuffers, screenUv), minimumRoughness);
    const float alpha = roughness * roughness;

    if (roughness > settings.iblFallbackRougnessThresholdEnd)
    {
        output[threadId.xy] = 0.0;
        return;
    }

    float2 randomVector = LoadBlueNoise(threadId.xy);

    #if 0 // use interleaved gradient noise intead of blue noise
    SeedRng(frameConstants.frameTimings.frameId);
    randomVector.x = InterleavedGradientNoise(threadId.xy + rand_pcg() % 1000);
    randomVector.y = InterleavedGradientNoise(threadId.xy + rand_pcg() % 1000);
    #endif
    randomVector.x *= (1 - settings.bias);
#if 1 
    float NDotV = dot(viewVectorWS, normalWS);
    if (NDotV <= 0.0f)
    {
        output[threadId.xy] = 0.0;
        return;
    }
#endif
    float3 directionWS = SampleGGXVNDFReflectionVector(-viewVectorWS, normalWS, alpha, randomVector); 
    float3 H = normalize(directionWS + viewVectorWS);

#if 1 
    float rDotV = dot(-viewVectorWS, normalize(directionWS));
    if (rDotV <= 0.0f)
    {
        output[threadId.xy] = 0.0;
        return; 
    }
#endif
    

    float LdotH = dot(directionWS, H);
    float NdotH = dot(normalWS, H);
    
    float pdf = GGXVNDF(NDotV, LdotH, NDotV, NdotH, alpha);

    const float3 originSS = float3(screenUv, nonlinearDepth);
    const float3 endPointWS = originWS + directionWS;
    const float3 endPointSS = WSToSS(endPointWS, cameraConstants.viewProjectionMatrix);
    const float3 directionSS = endPointSS - originSS;

    RayMarchSettings rayMarchSettings;
    rayMarchSettings.startMip = lowestMip;
    rayMarchSettings.startMipSize = rayHitBufferDimensions.size;
    rayMarchSettings.startMipTexelSize = rayHitBufferDimensions.texelSize;
    rayMarchSettings.maxMip = maxRayMarchingMip;
    rayMarchSettings.maxSteps = settings.maxHierarchicalSteps;
    rayMarchSettings.enableMarchBehind = false;

    const float3 rayHitSS = HierarchicalRayMarch(gBuffers.depthPyramide, originSS, directionSS, rayMarchSettings);

#if 0 
    float3 hitNormalWS = SampleNormalWS(gBuffers, rayHitSS.xy);
    float rDotN = dot(-normalize(directionWS), hitNormalWS);

    if (rDotN <= 0.0f)
    {
        output[threadId.xy] = 0.0;
        return; 
    }
#endif

#if 0 
    float2 manhattan_dist = abs(rayHitSS.xy - screenUv); //@note: from ffx
    if(all(manhattan_dist < 2.0f * rayHitBuffer.texelSize)) 
    {
        output[threadId.xy] = 0.0;
        return;
    }
#endif

#if 0
        float3 rayHitWS = SSToWS(rayHitSS.xy, rayHitSS.z, cameraConstants.inverseViewProjectionMatrix);

        //float3 reflectionRaySS = rayHitSS - originSS;
        //float4 reflectionRayWS = mul(float4(SSToNDC(reflectionRaySS.xy), reflectionRaySS.z, 0.0), cameraConstants.inverseViewProjectionMatrix);
        //reflectionRayWS /= reflectionRayWS.w;

        float rayHitSurfaceDepth = depthPyramide.SampleLevel(samplerPointClamp, rayHitSS.xy, 0);
        float3 rayHitSurfaceWS = SSToWS(rayHitSS.xy, rayHitSurfaceDepth, cameraConstants.inverseViewProjectionMatrix);
        const float distanceWS = distance(rayHitWS, rayHitSurfaceWS);
        if (distanceWS > settings.depthBufferThickness)
        {
            output[threadId.xy] = 0.0;
            return;
        }
#endif

    output[threadId.xy] = float4(rayHitSS, pdfScalingFactor * pdf);
}
