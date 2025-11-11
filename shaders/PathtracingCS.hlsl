#include "Common.hlsli"
#include "ColorHelpers.hlsli"
#include "ShadingHelpers.hlsli"
#include "SamplingHelpers.hlsli"
#include "RaytracingHelpers.hlsli"

struct RootConstants
{
    uint outputUavId;
    uint renderTargetDimensionsX;
    uint renderTargetDimensionsY;
    float renderTargetTexelSizeX;
    float renderTargetTexelSizeY;
    uint accelerationStructureSrvId;
    uint instanceGeometryDataOffset;
    uint skyBoxSrvId;
    uint lightingDataOffset;
};
ConstantBuffer<RootConstants> rootConstants : register(b0);

[numthreads(8, 8, 1)]
void main( uint3 threadId: SV_DispatchThreadID )
{
    const PathtracerSettings settings = frameConstants.settings.pathtracerSettings;
    const uint2 renderTargetDimensions = uint2(rootConstants.renderTargetDimensionsX, rootConstants.renderTargetDimensionsY);
    const float2 renderTargetTexelSize = float2(rootConstants.renderTargetTexelSizeX, rootConstants.renderTargetTexelSizeY);
    const bool useSpecular = frameConstants.settings.lightingSettings.useSpecular;

    CameraConstants cameraConstants = frameConstants.mainCameraData;
    LightingData lightingData = BufferLoad < LightingData> (rootConstants.lightingDataOffset);
    MaterialSettings materialSettings = frameConstants.settings.materialSettings;
    RaytracingAccelerationStructure accelerationStructure = ResourceDescriptorHeap[rootConstants.accelerationStructureSrvId];

    RWTexture2D<float4> output = ResourceDescriptorHeap[rootConstants.outputUavId];
    TextureCube skyBox = ResourceDescriptorHeap[rootConstants.skyBoxSrvId];

    uint seed = initRNG(threadId.xy, renderTargetDimensions, uint(frameConstants.frameTimings.frameId));
    #if 1 // use random subpixel offset
    float2 subPixelOffset = float2(rand(seed), rand(seed));
    #else // use Halton subpixel offset. //@note: Leads to increased aliasing
    float2 subPixelOffset = frameConstants.mainCameraData.subPixelJitter;
    #endif

    const float2 pixelPositon = float2(threadId.xy) + 0.5 + subPixelOffset; 

    const float2 positionNDC = SSToNDC(pixelPositon * renderTargetTexelSize.xy);

    const float aspectRatio = cameraConstants.projectionMatrix[1][1] / cameraConstants.projectionMatrix[0][0];
    const float focalLength = cameraConstants.projectionMatrix[1][1];

    RayDesc ray;
    ray.Origin = cameraConstants.position;
    ray.TMin = 0;
    ray.TMax = FLT_MAX;
    ray.Direction = normalize(CalculateWSCameraRay(positionNDC, cameraConstants.inverseViewMatrix, aspectRatio, focalLength));
    
    float3 accumulatedRadiance = output[threadId.xy].rgb;

    float3 throughput = 1.0;
    float3 radiance = 0.0;
    float primaryHitDistance = 0.0;

    for (int i = 0; i < settings.bounceMaximumCount; i++)
    {
        RayQuery < RAY_FLAG_CULL_BACK_FACING_TRIANGLES | //@note: this also helps to get rid of pattern on floor!
            RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> query;

        query.TraceRayInline(accelerationStructure, RAY_FLAG_NONE, uint(-1), ray);
        while (query.Proceed())
        {
            if (query.CandidateType() == CANDIDATE_NON_OPAQUE_TRIANGLE)
            {
                RaytracingHit rtHit = ExtractCandidateResult(query);
                HitData hitData = LoadHitData(rtHit, rootConstants.instanceGeometryDataOffset, materialSettings);
                if (hitData.material.albedo.a > 0.05)
                {
                    query.CommitNonOpaqueTriangleHit(); //@note: need to check all hits not only the first since the first is not necessarily the closest
                }
            }
        }

        if (query.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
        {
            //@note: need to actually reload all data here, since last committed hit is not necessarily closest
            RaytracingHit rtHit = ExtractCommittedResult(query);
            HitData hitData = LoadHitData(rtHit, rootConstants.instanceGeometryDataOffset, materialSettings);

            MaterialData materialData = hitData.material;
            HitGeometryData interpolatedVertexAttributes = hitData.interpolatedVertexAttributes;
            
            MaterialShadingData material = ComputeMaterialShadingData(materialData);

            if (i==0)
            {
                primaryHitDistance = rtHit.rayHitT;
            }

            #if 0 //emissive test
            float3 emissive = max(0, materialData.albedo - 0.90);
            radiance += throughput * 500 * emissive;
            #endif

            float3 viewVector = -ray.Direction;
            const float NdotV = max(0.0001, saturate(dot(materialData.normal, viewVector)));

            // light sampling
            const LightsData lightsData = lightingData.lightsData;
            uint totalLightsCount = lightsData.directionalLightsCount + lightsData.pointLightsCount + lightsData.shadowedPointLightsCount;
            if (totalLightsCount > 0)
            {
                bool castShadowRay = true;
                const float lightWeight =  totalLightsCount; 
                const uint directionalLightIndex = min(totalLightsCount - 1, rand(seed) * totalLightsCount); // min in case random numer is 1.0
                const uint pointLightIndex = directionalLightIndex - lightsData.directionalLightsCount;
                const uint shadowedPointLightIndex = pointLightIndex - lightsData.pointLightsCount;

                float3 lightIntensity;
                float3 lightDirection;
                float lightDistance = FLT_MAX;

                if (directionalLightIndex < lightsData.directionalLightsCount)
                {
                    ShadowedLight light = BufferLoad < ShadowedLight > (lightsData.directionalLightsBufferOffset, directionalLightIndex);
                    lightIntensity = light.color;
                    lightDirection = normalize(light.direction);
                }
                else if (pointLightIndex < lightsData.pointLightsCount)
                {
                    Light light = BufferLoad < Light > (lightsData.pointLightsBufferOffset, pointLightIndex);
                    lightDirection = light.position - interpolatedVertexAttributes.position;
                    lightDistance = max(0.00001, length(lightDirection));

                    if (settings.castShadowsForUnshadowedLights)
                    {
                        castShadowRay = castShadowRay && (lightDistance < light.fadeEnd); //@note: this helps to get rid of some nans
                    }
                    else
                    {
                        castShadowRay = false;
                    }

                    lightDirection /= lightDistance;
                    lightIntensity = light.color * AttenuateLight(lightDistance, light.fadeBegin, light.fadeEnd);
                }
                else
                {
                    ShadowedLight light = BufferLoad < ShadowedLight > (lightsData.shadowedPointLightsBufferOffset, shadowedPointLightIndex);
                    lightDirection = light.position - interpolatedVertexAttributes.position;
                    lightDistance = max(0.00001, length(lightDirection));

                    castShadowRay = castShadowRay && (lightDistance < light.fadeEnd); //@note: this helps to get rid of some nans

                    lightDirection /= lightDistance;
                    lightIntensity = light.color * AttenuateLight(lightDistance, light.fadeBegin, light.fadeEnd);
                }

                RayDesc ray;
                ray.Origin = interpolatedVertexAttributes.position; 
                ray.TMin = 0.001; //@note: this fixes stripe pattern on floor from cone
                ray.TMax = lightDistance;

                ray.Direction = lightDirection;

                RayQuery < RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES |
                    RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH > shadowQuery;

                if (castShadowRay)
                {
                    shadowQuery.TraceRayInline(accelerationStructure, RAY_FLAG_NONE, uint(-1), ray);
    
                    while (shadowQuery.Proceed())
                    {
                        if (shadowQuery.CandidateType() == CANDIDATE_NON_OPAQUE_TRIANGLE)
                        {
                            #if 0 // ignore alpha for shadow rays
                            shadowQuery.CommitNonOpaqueTriangleHit();
                            break;
                            #else
                            RaytracingHit rtHit = ExtractCandidateResult(shadowQuery);
                            HitData hitData = LoadHitData(rtHit, rootConstants.instanceGeometryDataOffset, materialSettings);
                            if (hitData.material.albedo.a > 0.05)
                            {
                                shadowQuery.CommitNonOpaqueTriangleHit();
                            }
                            #endif
                        }
                    }
                }

                if (!castShadowRay || !shadowQuery.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
                {
                    MaterialShadingData modifiedRoughnessMaterial = material;

                    modifiedRoughnessMaterial.roughness = settings.boostRoughness ? 
                        saturate(max(materialData.roughness, i * 0.5)) : 
                        materialData.roughness; //@note: this reduces fireflies but will introduce bias. There are more sophisticated versions of this (c.f. "Optimised Path Space Regularisation")

                    modifiedRoughnessMaterial.alpha = modifiedRoughnessMaterial.roughness * modifiedRoughnessMaterial.roughness;
                        
                    //@note: this uses a different diffuse from bounced diffuse
                    float3 brdf = CombinedBrdf(materialData.normal, viewVector, lightDirection, modifiedRoughnessMaterial, useSpecular, 1.0);

                    radiance += throughput * brdf * lightIntensity * lightWeight;
                }
            }

            //Russian Roulette Termination 
            if (i > 3)
            {
                float rr_p = min(0.95, Luminance(throughput)); 
                if (rr_p < rand(seed))
                {
                    break;
                }
                else
                {
                    throughput /= rr_p; 
                }
            }

            float specularLuminance = Luminance(Fresnel_Shlick(material.c_spec, 1.0, max(0.0, NdotV)));
            float diffuseLuminance = Luminance(material.c_diff.xyz);
            float p = (specularLuminance / max(0.0001, (specularLuminance + diffuseLuminance)));
            p = clamp(p, 0.1, 0.9);
            p = (materialData.metalness == 1.0 && materialData.roughness == 0.0) ? 1.0 : p;

            //sample next direction
            ray.Origin = interpolatedVertexAttributes.position;

            const float2 u = float2(rand(seed), rand(seed));

            float3 brdfWeight = 0.0;

            if (useSpecular && (rand(seed) < p ))
            {
                throughput /= p; 

                ray.Direction = SampleGGXVNDFReflectionVector(-viewVector, materialData.normal, material.alpha, u);

                ShadingProducts products = ComputeShadingProducts(materialData.normal, viewVector, ray.Direction);

                brdfWeight = Fresnel_Shlick(material.c_spec, 1.0, products.LdotH) *
                    Smith_G1_GGX(material.alpha, products.NdotL, material.alpha * material.alpha, products.NdotL * products.NdotL);
            }
            else
            {
                throughput /= useSpecular ? (1.0 - p) : 1.0; 
                float pdf;

                float3x3 TBNMatrix = (float3x3) Rotation(materialData.normal); //@note: TBN calculated from uvs not accurate enough!
                ray.Direction = SampleCosineWeightedHemisphere(u, pdf);
                ray.Direction = mul(ray.Direction, TBNMatrix); //@todo: hack the terminator according to ray tracing gems 2
                brdfWeight = material.c_diff; //@note: uses lambert diffuse, instead of Burley
            }

            throughput *= brdfWeight;
        }
        else
        {
            #if 1 // skybox environment lighting
            float3 skyColor = skyBox.SampleLevel(samplerLinearClamp, ray.Direction, 0).rgb;
            #else // white furnace environment lighting
            float skyColor = 1.0; 
            #endif 

            radiance += throughput * skyColor * frameConstants.settings.lightingSettings.skyBrightness;
            break;
        }
    }

    accumulatedRadiance = accumulatedRadiance * frameConstants.staticFramesCount + clamp(radiance, 0, FLT_MAX); //@note: clamp gets rid of NaNs that rarely appeared otherwise
    accumulatedRadiance /= (frameConstants.staticFramesCount + 1.0);

    output[threadId.xy] = float4(accumulatedRadiance, primaryHitDistance);
}

