#include "Common.hlsli"

#include "ClusteredShadingCommon.hlsli"
#include "GBuffers.hlsli"
#include "ShadingHelpers.hlsli"
#include "ViewHelpers.hlsli"

void ApplyDebugVisualizationSettings(MaterialData materialData,
    float ambientOcclusion,
    float2 velocity,
    float linearDepth,
    inout float3 litColor);

struct RootConstants 
{
    uint outputUavId;
    uint ssaoMapSrvId;
    uint diffuseBufferSrvId;
    uint specularBufferSrvId;
    uint lightingDataOffset;
};
ConstantBuffer<RootConstants> rootConstants : register(b0);

[numthreads(clusteredShadingTileSizeX, clusteredShadingTileSizeY, 1)]
void main( uint3 threadId: SV_DispatchThreadID )
{
    RenderFeatures renderFeatures = RenderFeaturesDefaults();
    renderFeatures.sampleAoMap = true;
    renderFeatures.sampleIndirectDiffuseMap = true;
    renderFeatures.sampleIndirectSpecularMap = true;
    renderFeatures.sampleSpecularCubeMap = true;

    RWTexture2D<float4> output = ResourceDescriptorHeap[rootConstants.outputUavId];

    CameraConstants cameraConstants = frameConstants.mainCameraData;

    LightingData lightingData = BufferLoad < LightingData> (rootConstants.lightingDataOffset);
     
    GBuffers gBuffers = LoadGBuffers(frameConstants.gBufferSrvIds);

    const float2 velocity = gBuffers.velocities.Load(int3(threadId.xy, 0));
    const float nonlinearDepth = gBuffers.depth.Load(int3(threadId.xy, 0));

    const float2 screenUv = float2(threadId.xy + 0.5) * frameConstants.mainRenderTargetDimensions.texelSize; 
    const float3 positionWS = SSToWS(screenUv, nonlinearDepth, cameraConstants.inverseViewProjectionMatrix);
    
    const float depthVS = NonlinearToLinearDepth(nonlinearDepth, cameraConstants.projectionMatrix);
    const uint3 clusterIndex = CalculateClusterIndex(threadId.xy, depthVS, cameraConstants);

    const float3 viewVector = normalize(cameraConstants.position.xyz - positionWS);

    MaterialData materialData = LoadMaterialDataFromGBuffers(gBuffers, threadId.xy);

    float ambientOcclusion = 1.0;
    float4 specularRadiance = 0.0;
    float3 indirectDiffuseIrradiance = 0.0;
    if (IsValidId(rootConstants.ssaoMapSrvId) && renderFeatures.sampleAoMap && frameConstants.settings.ssaoSettings.isActiveAO) 
    {
        float3 directionalOcclusion = 0.0;
        Texture2D ssaoTexture = ResourceDescriptorHeap[rootConstants.ssaoMapSrvId];
        ambientOcclusion = ssaoTexture.SampleLevel(samplerLinearClamp, screenUv, 0).a;
        #if 0
        directionalOcclusion = ssaoTexture.SampleLevel(samplerLinearClamp, screenUv, 0).rgb; 
        indirectDiffuseIrradiance.rgb = directionalOcclusion / PI; 
        #endif
    }

    if (IsValidId(rootConstants.specularBufferSrvId) && renderFeatures.sampleIndirectSpecularMap)
    {
        Texture2D<float4> specularBuffer = ResourceDescriptorHeap[rootConstants.specularBufferSrvId];
        specularRadiance = specularBuffer.Load(int3(threadId.xy, 0));
    }

    if (IsValidId(rootConstants.diffuseBufferSrvId))
    {
        renderFeatures.sampleDiffuseDDGI = false; //if per pixel indirect diffuse is used, do not sample ddgi probes

        Texture2DArray<float4> diffuseBuffer = ResourceDescriptorHeap[rootConstants.diffuseBufferSrvId];
        if (frameConstants.settings.indirectDiffuseSettings.denoiseInSHSpace)
        {
            const float4 radianceR = diffuseBuffer.Load(int4(threadId.xy, 0, 0));
            const float4 radianceG = diffuseBuffer.Load(int4(threadId.xy, 1, 0));
            const float4 radianceB = diffuseBuffer.Load(int4(threadId.xy, 2, 0));
            
            indirectDiffuseIrradiance.r += ComponentSum(radianceR * EvaluateSH4Basis(materialData.normal) * CosineSH4());
            indirectDiffuseIrradiance.g += ComponentSum(radianceG * EvaluateSH4Basis(materialData.normal) * CosineSH4());
            indirectDiffuseIrradiance.b += ComponentSum(radianceB * EvaluateSH4Basis(materialData.normal) * CosineSH4());
        }
        else
        {
            const float4 indirectDiffuseIrradianceAndAo = diffuseBuffer.Load(int4(threadId.xy, 0, 0));
            indirectDiffuseIrradiance = indirectDiffuseIrradianceAndAo.rgb;
            ambientOcclusion *= indirectDiffuseIrradianceAndAo.a; // a-component contains ray-traced ambient occlusion if denoising is not done in SH space
        }

        SetDebugIndirectDiffuse(indirectDiffuseIrradiance);
    }

    float3 litColor = LightSurface(viewVector,
        positionWS,
        materialData,
        ambientOcclusion,
        specularRadiance,
        indirectDiffuseIrradiance,
        frameConstants.settings.lightingSettings,
        renderFeatures,
        lightingData,
        clusterIndex);

    ApplyDebugVisualizationSettings(materialData, ambientOcclusion, velocity, depthVS, litColor);

    output[threadId.xy] = float4(litColor, 1.0);
}

void ApplyDebugVisualizationSettings(MaterialData materialData,
    float ambientOcclusion,
    float2 velocity,
    float linearDepth,
    inout float3 litColor)
{
    switch (frameConstants.settings.debugVisualizationSettings.renderMode)
    {
        case DebugVisualization::RenderMode::Ssao:
            litColor = ambientOcclusion; 
            break;
        case DebugVisualization::RenderMode::IndirectLighting:
            litColor = GetDebugIndirectDiffuse();
            break;
        case DebugVisualization::RenderMode::Normals:
            litColor = materialData.normal;
            break;
        case DebugVisualization::RenderMode::NormalsOctahedral:
            litColor = float3(OctahedralEncode(materialData.normal), 0.0);
            break;
        case DebugVisualization::RenderMode::Albedo:
            litColor = materialData.albedo.rgb;
            break;
        case DebugVisualization::RenderMode::Roughness:
            litColor = materialData.roughness;
            break;
        case DebugVisualization::RenderMode::Metalness:
            litColor = materialData.metalness;
            break;
        case DebugVisualization::RenderMode::Velocities:
            litColor = float3(velocity, 0.0);
            break;
        case DebugVisualization::RenderMode::DepthLinear:
            litColor = linearDepth;
            break;
        case DebugVisualization::RenderMode::ClusterLightCount:
        {
                const float value = float(GetDebugClusterCount()) / frameConstants.settings.debugVisualizationSettings.debugClusteredPerCellMaxCount;
                const float3 colorA = float3(0, 1, 0);
                const float3 colorB = float3(1, 0, 0);
                litColor.rgb = lerp(litColor.rgb, lerp(colorA, colorB, value), 0.7);
                break;
        }
        case DebugVisualization::RenderMode::CascadeIndex:
            litColor.rgb = lerp(litColor.rgb, GetDebugCascadeLevelColorHash(), 0.1); 
            break;
    }
}
