#define ALPHA_TESTED
#include "Common.hlsli"

#include "ClusteredShadingCommon.hlsli"
#include "GBuffers.hlsli"
#include "ShadingHelpers.hlsli"
#include "PBRMeshInterpolants.hlsli"
#include "ViewHelpers.hlsli"

struct RootConstants
{
    uint reserved0;
    uint materialConstantsOffset;
    uint reserved2;
    uint reserved3;
    uint ssaoMapSrvId;
    uint specularMapSrvId;
    uint indirectDiffuseMapSrvId;
    uint lightingDataOffset;
    uint renderFeatures;
    uint cameraConstantsOffset;
    uint renderTargetDimensionsOffset;
};
ConstantBuffer<RootConstants> rootConstants : register(b0);

[RootSignature(universalRS)]
float4 main(MeshInterpolants input) : SV_TARGET
{
    const LightingSettings lightingSettings = frameConstants.settings.lightingSettings;
    const MaterialSettings materialSettings = frameConstants.settings.materialSettings;
    const RenderFeatures renderFeatures = AsRenderFeatures(rootConstants.renderFeatures);

    CameraConstants cameraConstants = BufferLoad < CameraConstants > (rootConstants.cameraConstantsOffset);
    LightingData lightingData = BufferLoad < LightingData> (rootConstants.lightingDataOffset);
    TextureDimensions renderTargetDimensions = BufferLoad < TextureDimensions> (rootConstants.renderTargetDimensionsOffset);

    bool validScreenCoordinate = true;

    #ifdef ENABLE_DEBUG_CAMERA 
    if (renderFeatures.isDebugCamera)
    {
        cameraConstants = frameConstants.mainCameraData;
        input.position = mul(float4(input.positionWS, 1.0), cameraConstants.viewProjectionMatrix);
        input.position.xyz /= input.position.w;
        validScreenCoordinate = IsWithinNDCFrustum(input.position.xyz);
        input.position.xy = NDCToSS(input.position.xy) * frameConstants.mainRenderTargetDimensions.size;
        if (frameConstants.settings.debugVisualizationSettings.limitDebugCameraToMainCameraViewPort)
        {
            if (!validScreenCoordinate)
            {
                discard;
            }
        }
    }
    #endif

    uint3 clusterIndex = CalculateClusterIndex(input.position.xy, input.position.w, cameraConstants);
    
    MaterialConstants materialConstants = MaterialConstants::Init();
    if (IsValidOffset(rootConstants.materialConstantsOffset))
    {
        materialConstants = BufferLoad < MaterialConstants > (rootConstants.materialConstantsOffset);
    }
    ApplyMaterialSettings(materialSettings, materialConstants);

    const float3 viewVector = normalize(cameraConstants.position.xyz - input.positionWS);

    MaterialData materialData = ReadMaterialData(input.uv, viewVector, normalize(input.normal), materialConstants);

    #ifdef ALPHA_TESTED
    clip(materialData.albedo.a - 0.05f);
    #endif

    #ifdef DEBUG_PER_PIXEL_FACE_NORMAL
   	float3 vertex0 = GetAttributeAtVertex(input.positionWS2, 0);
	float3 vertex1 = GetAttributeAtVertex(input.positionWS2, 1);
	float3 vertex2 = GetAttributeAtVertex(input.positionWS2, 2);
    materialData.normal = cross(vertex1 - vertex0, vertex2 - vertex0);
    #endif
    
    const float2 screenUv = input.position.xy * renderTargetDimensions.texelSize;

    #if 0 //use depth buffer to only show what the main camera sees, but leads to shadow acne like artifacts. Issue could likely be resolved in a similar way
    #ifdef ENABLE_DEBUG_CAMERA 
    if (renderFeatures.isDebugCamera)
    {
        GBuffers gBuffers = LoadGBuffers(frameConstants.gBufferSrvIds);
        float depthBufferDepth= gBuffers.depth.SampleLevel(samplerPointClamp, screenUv, 0);
        if (input.position.w > NonlinearToLinearDepth(depthBufferDepth, frameConstants.mainCameraData.projectionMatrix) + 1e-1)
        {
            discard;
        }
    }
    #endif
    #endif

    float ambientOcclusion = 1.0f;
    if (IsValidId(rootConstants.ssaoMapSrvId) && renderFeatures.sampleAoMap)
    {
        Texture2D ssaoTexture = ResourceDescriptorHeap[rootConstants.ssaoMapSrvId];
        ambientOcclusion = validScreenCoordinate ? ssaoTexture.SampleLevel(samplerLinearClamp, screenUv, 0).r : ambientOcclusion;
        
        if (frameConstants.settings.debugVisualizationSettings.renderMode == DebugVisualization::RenderMode::Ssao)
        {
            return ambientOcclusion.rrrr;
        }
    }

    float4 specularRadiance = 0.0;
    if (IsValidId(rootConstants.specularMapSrvId) && renderFeatures.sampleIndirectSpecularMap)
    {
        Texture2D<float4> specularMap = ResourceDescriptorHeap[rootConstants.specularMapSrvId];
        specularRadiance = validScreenCoordinate ? specularMap.SampleLevel(samplerLinearClamp, screenUv, 0) : specularRadiance;
    }
    
    float3 litColor = LightSurface(viewVector,
        input.positionWS,
        materialData,
        ambientOcclusion,
        specularRadiance,
        0.0,
        lightingSettings,
        renderFeatures,
        lightingData,
        clusterIndex);

    return float4(litColor, materialData.albedo.a);
}

