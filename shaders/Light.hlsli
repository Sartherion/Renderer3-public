#pragma once

struct Light
{
    float3 color;
    float3 position;
    float3 direction;
    float fadeBegin;
    float fadeEnd;
};

struct ShadowedLight : Light
{
    uint transformsOffset;
    uint shadowMapArrayBaseIndex;
};

struct LightsData
{
	uint pointLightsCount;
	uint pointLightsBufferOffset;
    uint shadowedPointLightsCount;
    uint shadowedPointLightsBufferOffset;
    uint omnidirectionalShadowMapsSrvId;
    uint directionalLightsCount;
    uint directionalLightsBufferOffset;
    uint cascadeShadowMapsSrvId;
    uint cascadeDataOffset;
    uint cascadeCount;
};

struct LightingData
{
    LightsData lightsData;
    uint clusterDataOffset;
    uint ddgiDataOffset;
    uint cubeMapSpecularId;
};