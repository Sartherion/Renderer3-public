#include "SSSRCommon.hlsli"
#include "SamplingHelpers.hlsli"
#include "ViewHelpers.hlsli"

//#define LIGHT_FROM_SCRATCH
#define FADE_THICKNESS_IN_LIGHTING_PASS
#define FADE_BORDER
//#define FADE_PERPENDICULAR
#define USE_CONE_SAMPLING
#define USE_TEMPORAL_REPROJECTION
#define HISTORY_SPECULAR_DEPTH_NORMAL_REJECTION 
#define HISTORY_LIT_DEPTH_NORMAL_REJECTION //@note: removes ghosting, but also prevents history reuse for thickness occlusion, unless motion_vector_base_reprojection ist activated
//#define MOTION_VECTOR_BASED_REPROJECTION
#define VIRTUAL_HIT_POINT_BASED_REPROJECTION
#define USE_REPROJECTION_COLOR_CLIP //@note: helps a lot with ghosting, but introduces temporal instability in regions not visible in reprojeted buffer

float ComputeDepthNormalSimilarity(float3 normal, float3 previousNormal, float linearDepth, float previousLinearDepth);

#ifdef USE_HALF_RESOLUTION_RAY_HIT_BUFFER 
static const uint sample_count = 4;
static const int2 sample_offsets[] = { int2(0, 0), int2(0, 1), int2(1, 0), int2(1, 1) };
#else
static const uint sample_count = 5;
static const int2 sample_offsets[] = { int2(0, 0), int2(0, 1), int2(-2, 1), int2(2, -3), int2(-3, 0), int2(1, 2), int2(-1, -2), int2(3, 0), int2(-3, 3),
                                   int2(0, -3), int2(-1, -1), int2(2, 1),  int2(-2, -2), int2(1, 0), int2(0, 2),   int2(3, -1)};
#endif

struct RootConstants
{
    uint outputUavId;
    uint rayHitBufferSrvId;
    uint specularHistoryBufferSrvId;
    uint previousLitBufferSrvId;
    uint lightingDataBufferOffset;
};
ConstantBuffer<RootConstants> rootConstants : register(b0);

[numthreads(8, 8, 1)]
void main(uint3 threadId : SV_DispatchThreadID)
{
    CameraConstants cameraConstants = frameConstants.mainCameraData;
    CameraConstants previousCameraConstants = frameConstants.previousMainCameraData;
    const GBuffers gBuffers = LoadGBuffers(frameConstants.gBufferSrvIds);

    RWTexture2D<float4> output = ResourceDescriptorHeap[rootConstants.outputUavId]; 

    Texture2D<float4> specularHistoryBuffer = ResourceDescriptorHeap[rootConstants.specularHistoryBufferSrvId];
    Texture2D rayHitBuffer = ResourceDescriptorHeap[rootConstants.rayHitBufferSrvId]; 

    Texture2D previousLitBuffer = ResourceDescriptorHeap[rootConstants.previousLitBufferSrvId];

    float historyWeight = settings.historyWeight;

    float2 screenUv = float2(threadId.xy + 0.5) * specularBufferDimensions.texelSize;
    float nonlinearDepth = gBuffers.depthPyramide.Load(int3(threadId.xy, 0)).r;
    float linearDepth = NonlinearToLinearDepth(nonlinearDepth, cameraConstants.projectionMatrix);
    float3 positionWS = SSToWS(screenUv, nonlinearDepth, cameraConstants.inverseViewProjectionMatrix);
    float surfaceDepth = length(cameraConstants.position.xyz - positionWS); 

    float3 albedo = gBuffers.albedo.Load(int3(threadId.xy, 0)).xyz;

    float3 normalWS = LoadNormalWS(gBuffers, threadId.xy);
    float3 viewVectorWS = normalize(cameraConstants.position.xyz - positionWS);
    const float NdotV = saturate(dot(normalWS, viewVectorWS));

    float roughness = max(LoadRoughness(gBuffers, threadId.xy), minimumRoughness);
    float alpha = roughness * roughness;
    float specularPower = RoughnessToSpecularPower(roughness);
    float coneTheta = SpecularPowerToConeAngle(specularPower) * 0.5f;

    float4 totalColor = float4(0.0f, 0.0f, 0.0f, 0.0f);
    float weightSum = 0;


    float avgReflectionRayLength = 0; 
    float3 avgReflectionRayWS = 0; 
    float2 avgRayHitSS = 0;

    float fadeOnThickness = 0.0;

    #ifdef USE_REPROJECTION_COLOR_CLIP 
    float3 m1 = 0;
    float3 m2 = 0;
    float3 neighborhoodMin = FLT_MAX;
    float3 neighborhoodMax = -FLT_MAX;
    uint contributingRayCount = 0;
    #endif

    for (uint i = 0; i < sample_count; i++) 
    {
        int3 rayHitPixelCoordinates = int3(threadId.xy + sample_offsets[i], 0);
        #ifdef USE_HALF_RESOLUTION_RAY_HIT_BUFFER 
        uint2 baseCoordinates = uint2((threadId.xy + 0.5) * 0.5 - 0.5); 
        rayHitPixelCoordinates = int3(baseCoordinates + sample_offsets[i], 0);
        #endif
        
        float4 rayHitBufferSample = rayHitBuffer.Load(rayHitPixelCoordinates);

        float pdf = rayHitBufferSample.w * rcp(pdfScalingFactor);
        if (pdf <= 0)
        {
            continue; 
        }

        float3 rayHitSS = rayHitBufferSample.xyz;
        float3 rayHitWS = SSToWS(rayHitSS.xy, rayHitSS.z, cameraConstants.inverseViewProjectionMatrix);

        #ifdef FADE_THICKNESS_IN_LIGHTING_PASS 
        float rayHitSurfaceDepth = gBuffers.depthPyramide.SampleLevel(samplerPointClamp, rayHitSS.xy, 0).r;
        float3 rayHitSurfaceWS = SSToWS(rayHitSS.xy, rayHitSurfaceDepth, cameraConstants.inverseViewProjectionMatrix);

        float dist = distance(rayHitWS, rayHitSurfaceWS);
        float sampleFadeOnThickness = 1.0f - smoothstep(0.0f, settings.depthBufferThickness, dist); //@note: adapted from AMD FFX
        fadeOnThickness = max(fadeOnThickness, sampleFadeOnThickness * sampleFadeOnThickness);
        #endif

        float2 motionVector = gBuffers.velocities.SampleLevel(samplerLinearClamp, rayHitSS.xy, 0) * float2(0.5, -0.5);

        float3 reflectionRayWS = rayHitWS - positionWS;

        float3 sampleColor = 0;
    
        float2 reprojectedRayHitPositionUv = rayHitSS.xy - motionVector;
            #ifdef HISTORY_LIT_DEPTH_NORMAL_REJECTION  
            float hitLinearDepth = NonlinearToLinearDepth(rayHitSS.z, cameraConstants.projectionMatrix);
            float3 previousHitNormalWS = OctahedralDecode(gBuffers.previousNormals.SampleLevel(samplerLinearClamp, reprojectedRayHitPositionUv, 0).rg);
            float3 hitNormalWS = SampleNormalWS(gBuffers, rayHitSS.xy);
            float previousHitDepth = gBuffers.previousDepth.SampleLevel(samplerLinearClamp, reprojectedRayHitPositionUv, 0);
            float previousHitLinearDepth = NonlinearToLinearDepth(previousHitDepth, previousCameraConstants.projectionMatrix); 

            float depthNormalSimilarity = ComputeDepthNormalSimilarity(hitNormalWS, previousHitNormalWS, hitLinearDepth, previousHitLinearDepth);
            if (depthNormalSimilarity < 0.5) 
            {
                continue;
            }
            #endif

        #ifdef USE_CONE_SAMPLING
        //@note: adapted from https://willpgfx.com/2015/07/screen-space-glossy-reflections/
        float2 deltaP = rayHitSS.xy - screenUv.xy; 
        float adjacentLength = length(deltaP);
        float2 adjacentUnit = normalize(deltaP);
        float oppositeLength = IsoscelesTriangleOpposite(adjacentLength, coneTheta) * lerp(0.25, 0.9, settings.bias); 
        #if 1
        oppositeLength *= lerp(saturate(NdotV * 2.0f), 1.0, sqrt(roughness)); //@todo: this is probably not how this term from Stachowiak is supposed to be used?
        #endif

        float2 samplePos = screenUv.xy + adjacentUnit * adjacentLength;
        const uint2 hdrBufferDimensions = specularBufferDimensions.size; 
        float mipLevel = clamp(log2(oppositeLength * max(hdrBufferDimensions.x, hdrBufferDimensions.y)), 0.0f, settings.maxConetracingMip);

        float2 historyUv = samplePos - motionVector;
        if (all(historyUv == saturate(historyUv)))
        {
            samplePos = historyUv;
        }
        else
        {
            continue; //@todo: leads to more black leaking into good regions
        }
        sampleColor = previousLitBuffer.SampleLevel(samplerTrilinearClamp, samplePos, mipLevel).rgb;

        #else
        sampleColor = previousLitBuffer.SampleLevel(samplerPointClamp, reprojectedRayHitPositionUv.xy, 0);
        #endif

        #ifdef LIGHT_FROM_SCRATCH  //instead of fetching radiance from previous frame hdr buffer, do complete relighting just from hit position 
        MaterialData materialData;
        materialData.albedo = gBuffers.albedo.SampleLevel(samplerLinearClamp, rayHitSS.xy, 0);
        materialData.normal = SampleNormalWS(gBuffers, rayHitSS.xy);
        materialData.metalness = LoadMetalness(gBuffers, rayHitSS.xy);
        materialData.roughness = LoadRoughness(gBuffers, rayHitSS.xy); 

        uint2 pixelPosition = rayHitSS.xy * rayHitBufferDimensions.size;
        float linearDepth = NonlinearToLinearDepth(rayHitSS.z, cameraConstants.projectionMatrix);
        uint3 clusterIndex = CalculateClusterIndex(pixelPosition, linearDepth, cameraConstants); 
       
        LightingData lightingData = BufferLoad<LightingData>(rootConstants.lightingDataBufferOffset);
        RenderFeatures renderFeatures = RenderFeaturesDefaults(); 
        //renderFeatures.forceLastShadowCascade = true;
        renderFeatures.useDiffuseIbl = frameConstants.settings.ddgiSettings.higherBounceIndirectDiffuseIntensity > 0 ? true : false;
        renderFeatures.useIndirectSpecular = frameConstants.settings.ddgiSettings.higherBounceIndirectSpecularIntensity > 0 ? true : false;

        sampleColor = LightSurface(-normalize(reflectionRayWS), rayHitWS, materialData, 1.0, 0.0, 0.0, frameConstants.settings.lightingSettings, renderFeatures, lightsData, clusterIndex);
        if (any(IsNaN(sampleColor))) //@todo: need to investigate where these NaNs come from 
        {
            continue;
        }
       #endif

        float rDotV = dot(-viewVectorWS, normalize(reflectionRayWS));
        float weight = Specular_D_GGX(alpha, rDotV) / pdf;

        totalColor.rgb += weight * sampleColor;
        weightSum += weight;

        avgReflectionRayWS += reflectionRayWS * weight;
        avgRayHitSS += rayHitSS.xy * weight;

        float ray_length = surfaceDepth + length(reflectionRayWS);
        avgReflectionRayLength += ray_length * weight; 

        #ifdef USE_REPROJECTION_COLOR_CLIP 
        contributingRayCount++;
        m1 += sampleColor;
        m2 += sampleColor * sampleColor;
        neighborhoodMin = min(sampleColor, neighborhoodMin);
        neighborhoodMax = max(sampleColor, neighborhoodMax);
        #endif
    }

    bool colorValid = false; 
    bool anyRayValid = false;
    if (weightSum > 0) 
    {
        anyRayValid = true;
        totalColor.rgb *= settings.intensity / weightSum;
        avgReflectionRayLength /= weightSum;
        avgReflectionRayWS /= weightSum;
        avgRayHitSS /= weightSum;

        #ifdef FADE_PERPENDICULAR 
        float rDotV = dot(-viewVectorWS, normalize(avgReflectionRayWS));
        float fadeOnPerpendicular = saturate(lerp(0.0f, 1.0f, saturate(-rDotV * 4.0f))); 
        #else
        float fadeOnPerpendicular = 1.0;
        #endif
    
        #ifndef FADE_THICKNESS_IN_LIGHTING_PASS 
        fadeOnThickness = 1.0;
        #endif

        #ifdef FADE_BORDER 
        float2 boundary = abs(avgRayHitSS.xy - float2(0.5f, 0.5f)) * 2.0f;
        const float fadeDiffRcp = 1.0f / (settings.borderFadeEnd - settings.borderFadeStart);
        float fadeOnBorder = 1.0f - saturate((boundary.x - settings.borderFadeStart) * fadeDiffRcp);
        fadeOnBorder *= 1.0f - saturate((boundary.y - settings.borderFadeStart) * fadeDiffRcp);
        fadeOnBorder = smoothstep(0.0f, 1.0f, fadeOnBorder);
        #else
        float fadeOnBorder = 1.0;
        #endif

        float totalFade = fadeOnThickness * fadeOnBorder * fadeOnPerpendicular;
        float3 fadedColor = totalColor.rgb * totalFade; 
        if (any(fadedColor > 0))
        {
            colorValid = true;
            totalColor.a = totalFade;
        }
    }

    historyWeight = colorValid ? historyWeight : 1.0;

    #ifdef USE_TEMPORAL_REPROJECTION 
    float4 historyColorSpecular = 0;
    bool virtualReprojectionHistoryValid = false;
    bool surfaceReprojectionHistoryValid = false;
    float2 reprojectedUv = -1;

    float2 motionVector = gBuffers.velocities[threadId.xy] * float2(0.5, -0.5);
    float2 surfaceReprojectedUv = screenUv - motionVector; 
    float previousSurfaceDepth = gBuffers.previousDepth.SampleLevel(samplerLinearClamp, surfaceReprojectedUv, 0);
    float previousSurfaceLinearDepth = NonlinearToLinearDepth(previousSurfaceDepth, previousCameraConstants.projectionMatrix);

    #ifdef HISTORY_SPECULAR_DEPTH_NORMAL_REJECTION 
        float3 previousNormalWS = OctahedralDecode(gBuffers.previousNormals.SampleLevel(samplerLinearClamp, surfaceReprojectedUv, 0).rg);

        float depthNormalSimilarity = ComputeDepthNormalSimilarity(normalWS, previousNormalWS, linearDepth, previousSurfaceLinearDepth);
    if (depthNormalSimilarity > settings.depthNormalSimilarityThreshold)
    #endif
    {
    #ifdef VIRTUAL_HIT_POINT_BASED_REPROJECTION
        float4 virtualHistorySpecular = 0;
        float3 virtualRayHitPositionWS = cameraConstants.position.xyz + avgReflectionRayLength * (-viewVectorWS);
        float2 virtualReprojectedUv = WSToSS(virtualRayHitPositionWS, previousCameraConstants.viewProjectionMatrix).xy;  

        if (anyRayValid && all(virtualReprojectedUv == saturate(virtualReprojectedUv)))
        {
            virtualHistorySpecular = specularHistoryBuffer.SampleLevel(samplerLinearClamp, virtualReprojectedUv, 0); //@note: using point sampler instead of linear reduces ghosting artifacts! But it does lead to wobbly reprojection
            virtualReprojectionHistoryValid = virtualHistorySpecular.a > 0.0;


            if (virtualReprojectionHistoryValid)
            {
                historyColorSpecular = virtualHistorySpecular;
                reprojectedUv = virtualReprojectedUv;
            }
        }
    #endif

    #ifdef MOTION_VECTOR_BASED_REPROJECTION
    float4 surfaceHistorySpecular = 0;
        float3 previousSurfacePosition = SSToWS(surfaceReprojectedUv, previousSurfaceDepth, previousCameraConstants.inverseViewProjectionMatrix);
        float3 cameraDelta = cameraConstants.position - previousCameraConstants.position;
        float parallax = ComputeParallax(positionWS, previousSurfacePosition, cameraDelta);
        surfaceReprojectedUv = parallax < settings.parallaxThreshold ? surfaceReprojectedUv : -1; 

        if (!virtualReprojectionHistoryValid && all(surfaceReprojectedUv == saturate(surfaceReprojectedUv)))
        {
            surfaceHistorySpecular = specularHistoryBuffer.SampleLevel(samplerLinearClamp, surfaceReprojectedUv, 0);
            surfaceReprojectionHistoryValid = surfaceHistorySpecular.a > 0.0;
        }

        if (surfaceReprojectionHistoryValid)
        {
            historyColorSpecular = surfaceHistorySpecular;
            reprojectedUv = surfaceReprojectedUv;
        }
    #endif
    }

    #ifdef USE_REPROJECTION_COLOR_CLIP //remove ghosting via color clip. 
    float normalization = 1.0 / contributingRayCount;
    float gamma = 1.0f;
    float3 mu = m1 * normalization;
    float3 sigma = sqrt(abs((m2 * normalization) - mu * mu));
    float3 minc = mu - gamma * sigma;
    float3 maxc = mu + gamma * sigma;
    //virtualHistorySpecular.rgb = clamp(historyColorSpecular.rgb, neighborhoodMin, neighborhoodMax); //clamp leads to ugly artifacts
    float3 debugHistoryColor = historyColorSpecular.rgb;
    float normalizedDifference = any(sigma > 0) ? length(historyColorSpecular.rgb - mu) * length(historyColorSpecular.rgb - mu) / length(sigma * sigma) : 0;
    historyColorSpecular.rgb = ClipAABB(minc, maxc, historyColorSpecular.rgb);
    #endif

    bool anyHistoryValid = virtualReprojectionHistoryValid || surfaceReprojectionHistoryValid;
    historyWeight = anyHistoryValid ? historyWeight : 0.0;
    
    totalColor.a = colorValid ? totalColor.a : historyColorSpecular.a;
    totalColor.rgb = lerp(totalColor.rgb, historyColorSpecular.rgb, historyWeight);
    #endif

    float fallbackBlendWeight = saturate((settings.iblFallbackRougnessThresholdEnd - roughness) / (settings.iblFallbackRougnessThresholdEnd - settings.iblFallbackRougnessThresholdBegin));
    totalColor.a = lerp(0, totalColor.a, fallbackBlendWeight);

    output[threadId.xy] = clamp(totalColor, 0, FLT_MAX);
}

float ComputeDepthNormalSimilarity(float3 normal, float3 previousNormal, float linearDepth, float previousLinearDepth)
{
    //@note: taken from ffx
    return 1.0f * exp(-abs(1.0f - max(0.0, dot(normal, previousNormal))) * 1.4) * exp(-abs(previousLinearDepth - linearDepth) / linearDepth * 1.0);
}

