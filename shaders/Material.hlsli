#pragma once
#include "../SharedDefines.h"
#include "GBuffers.hlsli"
#include "MathHelpers.hlsli"
#include "UISettings.hlsli"

static const uint specularCubeMapsArrayIndexInvalid = specularCubeMapsArrayMaxIndex;
struct MaterialData
{
    float4 albedo;
    float3 normal; 
    float roughness;
    float metalness;
    uint specularCubeMapsArrayIndex;
};

struct MaterialConstants
{
    float4 albedo;
    float metalness;
    float roughness;
    uint albedoTextureId;
    uint normalTextureId;
    uint roughnessTextureId;
    uint metallicTextureId;
    uint specularCubeMapsArrayIndex;
    
    static MaterialConstants Init()
    {
        MaterialConstants materialConstants;
        materialConstants.albedo = 1.0;
        materialConstants.metalness = -1.0;
        materialConstants.roughness = -1.0;
        materialConstants.albedoTextureId = InvalidId;
        materialConstants.normalTextureId = InvalidId;
        materialConstants.roughnessTextureId = InvalidId;
        materialConstants.metallicTextureId = InvalidId;
        materialConstants.specularCubeMapsArrayIndex = specularCubeMapsArrayIndexInvalid;
        return materialConstants;
    }
};

void ApplyMaterialSettings(MaterialSettings materialSettings, inout MaterialConstants materialConstants)
{
    materialConstants.roughness = materialConstants.roughness < 0 ? materialSettings.defaultRoughness : materialConstants.roughness;
    materialConstants.metalness = materialConstants.metalness < 0 ? materialSettings.defaultMetalness: materialConstants.metalness;
    materialConstants.albedoTextureId = materialSettings.useAlbedoMaps ? materialConstants.albedoTextureId : InvalidId;
    materialConstants.normalTextureId = materialSettings.useNormalMaps ? materialConstants.normalTextureId : InvalidId;
    materialConstants.roughnessTextureId = materialSettings.useRoughnessMaps ? materialConstants.roughnessTextureId : InvalidId;
    materialConstants.metallicTextureId = materialSettings.useMetallicMaps ? materialConstants.metallicTextureId : InvalidId;
}

MaterialData LoadMaterialDataFromGBuffers(GBuffers gBuffers, uint2 pixelPosition)
{
    const float4 albedo = gBuffers.albedo.Load(int3(pixelPosition, 0));
    MaterialData materialData;
    materialData.albedo.rgb = albedo.rgb;
    materialData.normal = LoadNormalWS(gBuffers, pixelPosition);
    materialData.metalness = LoadMetalness(gBuffers, pixelPosition);
    materialData.roughness = LoadRoughness(gBuffers, pixelPosition);
    materialData.specularCubeMapsArrayIndex = albedo.a * specularCubeMapsArrayMaxIndex; 

    return materialData;
}

MaterialData SampleMaterialDataFromGBuffers(GBuffers gBuffers, float2 uv)
{
    MaterialData materialData;
    materialData.albedo.rgb = gBuffers.albedo.SampleLevel(samplerLinearClamp, uv, 0).rgb;
    materialData.normal = SampleNormalWS(gBuffers, uv);
    materialData.metalness = SampleMetalness(gBuffers, uv);
    materialData.roughness = SampleRoughness(gBuffers, uv);
    materialData.specularCubeMapsArrayIndex = specularCubeMapsArrayMaxIndex;

    return materialData;
}

MaterialData ReadMaterialData(float2 uv, float3 viewVector, float3 geometryNormal, MaterialConstants materialConstants) 
{
    MaterialData materialData;
    materialData.albedo = materialConstants.albedo;
    materialData.normal = geometryNormal;
    materialData.roughness = materialConstants.roughness;
    materialData.metalness = materialConstants.metalness;
    materialData.specularCubeMapsArrayIndex = materialConstants.specularCubeMapsArrayIndex;

    if (IsValidId(materialConstants.albedoTextureId))
    {
        Texture2D albedoTexture = ResourceDescriptorHeap[materialConstants.albedoTextureId];
        materialData.albedo = albedoTexture.Sample(samplerAnistropicWrap, uv);
        
    }
    if (IsValidId(materialConstants.roughnessTextureId))
    {
        Texture2D roughnessTexture = ResourceDescriptorHeap[materialConstants.roughnessTextureId];
        materialData.roughness = roughnessTexture.Sample(samplerAnistropicWrap, uv).r;
    }

    if (IsValidId(materialConstants.metallicTextureId))
    {
        Texture2D metallicTexture = ResourceDescriptorHeap[materialConstants.metallicTextureId];
        materialData.metalness = metallicTexture.Sample(samplerAnistropicWrap, uv).r;
    }

    if (IsValidId(materialConstants.normalTextureId))
    {
        Texture2D normalTexture = ResourceDescriptorHeap[materialConstants.normalTextureId];
        float3 normalMapSample = normalTexture.Sample(samplerAnistropicWrap, uv).xyz;
        float3x3 TBN = CalculateCotangentFrame(materialData.normal, -viewVector, uv);

        materialData.normal = PerturbNormal(normalMapSample, TBN);
    }

    return materialData;
}
