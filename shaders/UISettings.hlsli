#pragma once

struct LightingSettings
{
    float iblSpecularIntensity;
    float iblDiffuseIntensity;
    float skyBrightness;
    float shadowDarkness;
    int useSpecular : 8; 
};

struct MaterialSettings 
{
    int useAlbedoMaps: 8;
    int useNormalMaps: 8;
    int useRoughnessMaps: 8;
    int useMetallicMaps: 8;
    float defaultRoughness;
    float defaultMetalness;
};

struct PathtracerSettings
{
    int isActive: 8;
    int denoise: 8;
    int boostRoughness: 8;
    int castShadowsForUnshadowedLights : 8;
    int bounceMaximumCount;
    float denoiseNormalSigma;
    float denoiseDepthSigma;
};

struct DebugVisualizationSettings
{
    uint renderMode;
    int debugCascadeIndex;
    int debugClusteredPerCellMaxCount;
    int isActiveDebugCamera : 8;
    int limitDebugCameraToMainCameraViewPort : 8;
    int isActiveDDGIVisualization : 8;
    int isRadianceDDGIVisualization : 8;
    int isActiveCSMVisualization : 8;
};

struct SSSRSettings
{
    int maxHierarchicalSteps;
    int maxConetracingMip;
    float intensity;
    float bias;
    float depthBufferThickness;
    float depthNormalSimilarityThreshold;
    float borderFadeStart;
    float borderFadeEnd;
    float parallaxThreshold;
    float historyWeight;
    float iblFallbackRougnessThresholdBegin;
    float iblFallbackRougnessThresholdEnd;
};

struct SSAOSettings
{
    float thickness;
    float radius;
    int stepsCount;
    int azimuthalDirectionsCount;
    float blurDepthSigma;
    float blurNormalSigma;
    int isActiveAO : 8;
    int useOffsetJitter : 8;
};

struct DDGISettings
{
    float higherBounceIndirectDiffuseIntensity;
    float higherBounceIndirectSpecularIntensity;
    float irradianceGammaExponent;
    float historyBlendWeight;
};

struct IndirectDiffuseSettings
{
    float rtaoHitDistanceThreshold;
    float blurRadius;
    float normalWeightExponent;
    float preBlurRadius;
    float disocclusionDepthThreshold;
    float historyFixBilateralDepthScale;
    int denoiseInSHSpace: 8;
    int resetHistoryThisFrame: 8;
    int isActive : 8;
};

struct UISettings
{
    MaterialSettings materialSettings;
    LightingSettings lightingSettings;
    PathtracerSettings pathtracerSettings;
    SSAOSettings ssaoSettings;
    SSSRSettings sssrSettings;
    DDGISettings ddgiSettings;
    IndirectDiffuseSettings indirectDiffuseSettings;
    DebugVisualizationSettings debugVisualizationSettings;
};