#pragma once
#include "Common.hlsli"

#include "ClusteredShadingCommon.hlsli"
#include "DebugVisualization.hlsli"
#include "DDGICommon.hlsli"
#include "Light.hlsli"
#include "Material.hlsli"
#include "PBRHelpers.hlsli"
#include "RenderFeatures.hlsli"
#include "ViewHelpers.hlsli"

ShadingProducts ComputeShadingProducts(float3 N, float3 V, float3 L)
{
    float3 H = normalize(L + V);

    ShadingProducts result;
    result.NdotV = saturate(dot(N, V));
    result.NdotL = saturate(dot(N, L));
    result.LdotH = saturate(dot(L, H));
    result.NdotH = saturate(dot(N, H));
    
    return result;
}

struct MaterialShadingData : MaterialData
{
    float alpha;
    float3 c_diff;
    float3 c_spec;
};

MaterialShadingData ComputeMaterialShadingData(MaterialData materialData, float ambientOcclusion = 1.0)
{
    MaterialShadingData result;
    result.albedo = materialData.albedo;
    result.normal = materialData.normal;
    result.metalness = materialData.metalness;
    result.roughness = materialData.roughness;
    result.specularCubeMapsArrayIndex = materialData.specularCubeMapsArrayIndex;
    result.alpha = materialData.roughness * materialData.roughness;
    result.c_diff = lerp(materialData.albedo.rgb, float3(0, 0, 0), materialData.metalness);
    result.c_spec = lerp(kSpecularCoefficient, materialData.albedo.rgb, materialData.metalness);

    return result;
}

float Smith_G1_GGX(float alpha, float NdotS, float alphaSquared, float NdotSSquared) {
    #if 1 
	return 2.0f / (sqrt(((alphaSquared * (1.0f - NdotSSquared)) + NdotSSquared) / NdotSSquared) + 1.0f);
    #else //approximation used by Unreal / Hammond. 

    return NdotS * rcp(lerp(1, NdotS, alpha / 2.0));
    #endif
}

//@note: including Jacobian
float GGXVNDF(float NdotL, float LdotH, float NdotV, float NdotH, float alpha)
{
    float G1 = Smith_G1_GGX(alpha, NdotV, alpha * alpha, NdotV * NdotV);
    float D = Specular_D_GGX(alpha, NdotH);
    return G1 * D /** (LdotH)*/ / (NdotV * 4.0 /** LdotH*/); 
}

float3 CombinedBrdf(float3 N,
    float3 V,
    float3 L,
    MaterialShadingData material,
    bool useSpecular = true,
    float shadowFactor = 1.0) 
{
    ShadingProducts products = ComputeShadingProducts(N, V, L);
    float diffuse_factor = Diffuse_Burley(products, material.roughness);
    float3 specular = Specular_BRDF(material.alpha, material.c_spec, products);

    return  shadowFactor * products.NdotL * (((material.c_diff * diffuse_factor) + specular * (shadowFactor < 1.0f || !useSpecular ? 0.0 : 1.0f)));
}

//@note: approximation given in ch. 32 of Ray Tracing Gems
float3 EnvDFGPolynomial( float3 specularColor, float roughness, float ndotv )
{
    float2x2 c0 = { { 0.99044, -1.28514 }, { 1.29678, -0.755907 } };
    float2x2 c1 = { { 0.0365463, 3.32707 }, { 9.0632, -9.04756 } };
    
    float3x3 d0 = { { 1, 2.92338, 59.4188 }, { 20.3225, -27.0302, 222.592 }, { 121.563, 626.13, 316.627 } };
    float3x3 d1 = { { 1, 3.59685, -1.36772 }, { 9.04401, -16.3174, 9.22949 }, { 5.56589, 19.7886, -20.2123 } };
    
    float alpha = roughness * roughness;
    float alpha3 = alpha * alpha * alpha;
    float ndotv2 = ndotv * ndotv;
    float ndotv3 = ndotv2 * ndotv;

    float bias = mul(float2(1.0, alpha), mul(c0, float2(1.0, ndotv))) / mul(float3(1.0, alpha, alpha3), mul(d0, float3(1.0, ndotv, ndotv3)));
    float scale = mul(float2(1.0, alpha), mul(c1, float2(1.0, ndotv))) / mul(float3(1.0, alpha, alpha3), mul(d1, float3(1.0, ndotv2, ndotv3)));
 
    return specularColor * scale + bias;
}

float AttenuateLight(float distance, float fadeBegin, float fadeEnd)
{
    float distanceSquared = distance * distance;
    //return 1.0 / distanceSquared;
    //float radiusSquared = fadeEnd * fadeEnd;
    //return 2.0f / (distanceSquared + radiusSquared + distance * sqrt(distanceSquared + radiusSquared));

    return saturate((fadeBegin - distance) / (fadeEnd - fadeBegin)); //@note: linear fallof not physically correct!
}

float AccumulateDirectionalLightshadowFactor(float3 positionWS, float4x4 shadowMapTransform, uint shadowMapArrayIndex, Texture2DArray shadowMap, float shadowDarkness)
{
    float3 positionLS = mul(float4(positionWS, 1.0f), shadowMapTransform).xyz;
    float2 uv = float2(0.5, -0.5) * positionLS.xy + 0.5;
    return lerp(1.0f, shadowMap.SampleCmpLevelZero(samplerShadowMap, float3(uv, shadowMapArrayIndex), positionLS.z), shadowDarkness).r; 
}

float PointLightShadowFactor(float3 L, float linearDistance, uint shadowMapArrayBaseIndex, float farZ, TextureCubeArray omnidirectionalShadowMap, float shadowDarkness)
{
    float3 faceNormal = GetCubeMapFaceNormal(L);

    float nonLinearDistance = LinearToNonlinearDepth(linearDistance * dot(faceNormal, normalize(L)), omnidirectionalShadowMapNearZ, farZ);

    return lerp(1.0f, omnidirectionalShadowMap.SampleCmpLevelZero(samplerShadowMap, float4(L, shadowMapArrayBaseIndex), nonLinearDistance), shadowDarkness).r;
}

float3 EvaluatePointLight(float3 V,
    float3 N,
    float3 positionWS,
    MaterialShadingData material,
    Light light,
    LightingSettings settings,
    RenderFeatures renderFeatures,
    LightsData lightsData,
    uint shadowMapArrayBaseIndex = uint(-1))
{
    if (saturate(dot(light.color, light.color)) < 0.001f)
    {
        return 0.0;
    }
        
    TextureCubeArray omnidirectionalShadowMap = ResourceDescriptorHeap[lightsData.omnidirectionalShadowMapsSrvId];

     // light vector (to light)
    float3 L = light.position - positionWS;

    const float distance = length(L);

    if (distance > light.fadeEnd)
    {
        return 0.0;
    }

    L /= distance;

    float shadowFactor = 1.0f;
    if (renderFeatures.sampleShadowMap && shadowMapArrayBaseIndex != uint(-1))
    {
        shadowFactor = PointLightShadowFactor(-L, distance, shadowMapArrayBaseIndex, light.fadeEnd, omnidirectionalShadowMap, settings.shadowDarkness);
    }

    float lightAttenuation = AttenuateLight(distance, light.fadeBegin, light.fadeEnd);

    return lightAttenuation * light.color * CombinedBrdf(N, V, L, material, settings.useSpecular, shadowFactor);
}

float3 AccumulateDirectionalLights(float3 N,
    float3 V,
    float3 positionWS,
    MaterialShadingData material,
    RenderFeatures renderFeatures,
    LightingSettings settings,
    LightsData lightsData)
{
    float3 outputColor = 0.0;
    for (int i = 0; i < lightsData.directionalLightsCount; i++)
    {
        ShadowedLight light = BufferLoad < ShadowedLight > (lightsData.directionalLightsBufferOffset, i);
        Texture2DArray cascadedShadowMap = ResourceDescriptorHeap[lightsData.cascadeShadowMapsSrvId];

        if (saturate(dot(light.color, light.color)) < 0.001f)
        {
            continue;
        }
        
        // light vector (to light)
        const float3 L = normalize(light.direction);
        
        //shadow factor
        float shadowFactor = 1.0f;
        if (renderFeatures.sampleShadowMap)
        {
            int cascadeIndex = lightsData.cascadeCount - 1;
            if (!renderFeatures.forceLastShadowCascade)
            {
                for (int j = 0; j < lightsData.cascadeCount; j++) 
                {

                    BoundingBox boundingBox = BufferLoad < BoundingBox> (lightsData.cascadeDataOffset, j);
                    if (IsInsideBoundingBox(positionWS, boundingBox))
                    {
                        cascadeIndex = j;
                        break;
                    }
                }
            }

            SetDebugCascadeLevel(cascadeIndex, i);

            float4x4 shadowMapViewProjectionMatrix = BufferLoad < float4x4 > (light.transformsOffset, cascadeIndex); 
            shadowFactor = AccumulateDirectionalLightshadowFactor(positionWS, shadowMapViewProjectionMatrix, cascadeIndex + light.shadowMapArrayBaseIndex, cascadedShadowMap, settings.shadowDarkness);
        }
        
        outputColor += light.color * CombinedBrdf(N, V, L, material, settings.useSpecular, shadowFactor);
    }

    return outputColor;
}

float3 AccumulatePointLights(float3 N,
    float3 V,
    float3 positionWS,
    MaterialShadingData material,
    RenderFeatures renderFeatures,
    LightingSettings settings,
    LightsData lightsData)
{
    float3 outputColor = 0.0;
    for (int i = 0; i < lightsData.shadowedPointLightsCount; i++)
    {
        ShadowedLight light = BufferLoad < ShadowedLight > (lightsData.shadowedPointLightsBufferOffset, i);

        outputColor += EvaluatePointLight(V,
            N,
            positionWS,
            material,
            light,
            settings,
            renderFeatures,
            lightsData,
            light.shadowMapArrayBaseIndex);
    }

    return outputColor;
}

float3 AccumulateClusteredLights(float3 N,
    float3 V,
    float3 positionWS,
    MaterialShadingData material,
    RenderFeatures renderFeatures,
    LightingSettings settings,
    LightingData lightingData,
    uint3 clusterIndex)
{
    float3 outputColor = 0.0;

    ClusterData clusterData = BufferLoad < ClusterData > (lightingData.clusterDataOffset);

    if (any(clusterIndex < 0) || any(clusterIndex > clusterData.clusterCount))
    {
        debugClusterCount = 0;
        return 0.0;
    }
     
    ByteAddressBuffer lightListHeadNodes = ResourceDescriptorHeap[clusterData.lightListHeadNodesId];
    ByteAddressBuffer linkedLightList = ResourceDescriptorHeap[clusterData.linkedLightListId];

    const uint headNodesListIndex = sizeof(uint) * (clusterIndex.x +
        clusterIndex.y * clusterData.clusterCount.x +
        clusterIndex.z * clusterData.clusterCount.x * clusterData.clusterCount.y); 

    uint lightIndex = lightListHeadNodes.Load<uint>(headNodesListIndex);
    
    debugClusterCount = 0;
    LightNode lightNode;
    if (lightIndex != -1)
    {
        lightNode = BufferLoad<LightNode>(linkedLightList, lightIndex);
        
        //@note: need to go through light types in OPPOSITE order to how they were added to linkded list
        //point lights 
        if (renderFeatures.useDirectPointLights)
        {
            while (lightNode.lightType == 1)
            {
                uint lightId = lightNode.lightId;
             

                ShadowedLight light = BufferLoad < ShadowedLight > (lightingData.lightsData.shadowedPointLightsBufferOffset, lightId);

                lightIndex = lightNode.next;

            
                outputColor += EvaluatePointLight(V,
                    N,
                    positionWS,
                    material,
                    light,
                    settings,
                    renderFeatures,
                    lightingData.lightsData,
                    light.shadowMapArrayBaseIndex);

                debugClusterCount++;

                if (lightIndex == -1)
                    break;
        
                lightNode = BufferLoad < LightNode > (linkedLightList, lightIndex);
            }

            while (lightNode.lightType == 0)
            {
                uint lightId = lightNode.lightId;
             

                Light light = BufferLoad < Light> (lightingData.lightsData.pointLightsBufferOffset, lightId);

                lightIndex = lightNode.next;

            
                outputColor += EvaluatePointLight(V,
                    N,
                    positionWS,
                    material,
                    light,
                    settings,
                    renderFeatures,
                    lightingData.lightsData);

                debugClusterCount++;

                if (lightIndex == -1)
                    break;
        
                lightNode = BufferLoad < LightNode > (linkedLightList, lightIndex);
            }
        }
    }

    SetDebugClusterCount(debugClusterCount);

    return outputColor;
}

float3 IndirectDiffuse(float3 positionWS,
    float3 V,
    float3 N,
    float3 perPixelIrradiance,
    float ambientOcclusion,
    MaterialShadingData materialShadingData,
    LightingData lightingData,
    RenderFeatures renderFeatures)
{
    float3 indirectDiffuse = 0;

    indirectDiffuse = materialShadingData.c_diff * ambientOcclusion * perPixelIrradiance; 
    if (renderFeatures.sampleDiffuseDDGI)
    {
        DDGIData ddgiData = BufferLoad < DDGIData > (lightingData.ddgiDataOffset);

        const float3 ddgiIrradiance = SampleDDGIGrid(positionWS, V, N, ddgiData) / PI; 

        SetDebugIndirectDiffuse(ddgiIrradiance);

        indirectDiffuse = materialShadingData.c_diff * ambientOcclusion * ddgiIrradiance; 
    }
    indirectDiffuse = clamp(indirectDiffuse, 0, FLT_MAX);

    return indirectDiffuse;
}

float3 IndirectSpecular(float3 positionWS,
    float3 V,
    float4 perPixelRadiance,
    MaterialShadingData materialShadingData,
    LightingData lightingData,
    RenderFeatures renderFeatures)
{
    const float NdotV = saturate(dot(materialShadingData.normal, V));
    const float3 R = reflect(-V, materialShadingData.normal);

    float3 specular_env = 0;
    float blendWeight = 0;
    if (renderFeatures.sampleSpecularCubeMap && materialShadingData.specularCubeMapsArrayIndex != specularCubeMapsArrayIndexInvalid)
    {
        TextureCubeArray cubeMapSpecular = ResourceDescriptorHeap[lightingData.cubeMapSpecularId];
        specular_env = materialShadingData.c_spec * Specular_IBL(R, materialShadingData.roughness, materialShadingData.specularCubeMapsArrayIndex, cubeMapSpecular);
        blendWeight = 0; // don't use sssr when cubemap is used

    }
    else if (renderFeatures.sampleSpecularDDGI)
    {
        DDGIData ddgiData = BufferLoad < DDGIData > (lightingData.ddgiDataOffset);
        specular_env = materialShadingData.c_spec * SampleDDGIGrid(positionWS, V, R, ddgiData, DDGIQuantity::Radiance); 
        blendWeight = perPixelRadiance.a;
    }

    specular_env = clamp(specular_env, 0, FLT_MAX);

    const float3 FG = EnvDFGPolynomial(materialShadingData.c_spec, materialShadingData.roughness, NdotV);
    return lerp(specular_env, FG * perPixelRadiance.xyz, blendWeight); //@note: leaving out FG term results in much higher variance, as observed by Stachowiak  
}

template <bool useClustering = true>
float3 LightSurface(float3 V,
    float3 positionWS,
    MaterialData materialData,
    float ambientOcclusion,
    float4 specularRadiance, //@note: specularRadiance.a contains blend weight
    float3 diffuseIrradiance,
    LightingSettings settings,
    RenderFeatures renderFeatures,
    LightingData lightingData,
    uint3 clusterIndex = 0)
{
    float3 outputColor = 0;

    MaterialShadingData materialShadingData = ComputeMaterialShadingData(materialData, ambientOcclusion);
    
    //Directional Lights
    if (renderFeatures.useDirectDirectionalLights)
    {
        outputColor += AccumulateDirectionalLights(materialData.normal,
            V,
            positionWS,
            materialShadingData,
            renderFeatures,
            settings,
            lightingData.lightsData);
    }

    //Other lights
    if (useClustering) 
    {
        outputColor += AccumulateClusteredLights(materialData.normal, V, positionWS, materialShadingData, renderFeatures, settings, lightingData, clusterIndex);
    }
    else
    {
        if (renderFeatures.useDirectPointLights)
        {
            outputColor += AccumulatePointLights(materialData.normal,
                V,
                positionWS,
                materialShadingData,
                renderFeatures,
                settings,
                lightingData.lightsData);
        }

    }

    // Add specular radiance 
    if (settings.useSpecular)
    {
        outputColor += IndirectSpecular(positionWS,
            V,
            specularRadiance,
            materialShadingData,
            lightingData,
            renderFeatures) * settings.iblSpecularIntensity; 
    }

    // Add diffuse irradiance
    outputColor += IndirectDiffuse(positionWS,
        V,
        materialData.normal,
        diffuseIrradiance,
        ambientOcclusion,
        materialShadingData,
        lightingData,
        renderFeatures) * settings.iblDiffuseIntensity;

    return outputColor;
}

