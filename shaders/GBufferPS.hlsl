#define ALPHA_TESTED
#include "Common.hlsli"
#include "../SharedDefines.h"

#include "FrameConstants.hlsli"
#include "Material.hlsli"
#include "PBRMeshInterpolants.hlsli"
#include "ViewHelpers.hlsli"

struct Output
{
    float4 albedo : SV_Target0;
    float4 normals : SV_Target1;
    float2 velocity : SV_Target2;
};

struct RootConstants 
{
    uint reserved0;
    uint materialConstantsOffset;
    uint reserved2;
    uint reserved3;
    uint unused4;
    uint unused5;
    uint unused6;
    uint unused7;
    uint unused8;
    uint cameraConstantsOffset;
};
ConstantBuffer<RootConstants> rootConstants : register(b0);

Output PackGBuffers(MaterialData materialData, float2 velocity);

[RootSignature(universalRS)]
Output main(MeshInterpolants input)
{
    MaterialSettings materialSettings = frameConstants.settings.materialSettings;
    CameraConstants cameraConstants = BufferLoad < CameraConstants > (rootConstants.cameraConstantsOffset);
    CameraConstants previousCameraConstants = BufferLoad < CameraConstants > (rootConstants.cameraConstantsOffset, 1);

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

    const float2 screenUv = (input.position.xy) * frameConstants.mainRenderTargetDimensions.texelSize;
    const float3 currentPositionNDC = float3(SSToNDC(screenUv), input.position.z);
    float4 previousPositionNDC = mul(float4(input.positionWS, 1.0), previousCameraConstants.viewProjectionMatrix);
    previousPositionNDC /= previousPositionNDC.w;

    const float2 velocity = (currentPositionNDC.xy - cameraConstants.subPixelJitter) - (previousPositionNDC.xy - previousCameraConstants.subPixelJitter);

    return PackGBuffers(materialData, velocity);
}

Output PackGBuffers(MaterialData materialData, float2 velocity)
{
    Output output;
    
    output.albedo.rgb = materialData.albedo.rgb;
    output.albedo.a = float(materialData.specularCubeMapsArrayIndex) / specularCubeMapsArrayMaxIndex;
    output.normals.a = materialData.roughness; //store roughness in alpha component of normals RT
    output.normals.b = materialData.metalness; //store metallness in blue component of normals RT
    output.normals.rg = OctahedralEncode(materialData.normal); 
    output.velocity = velocity;

    return output;
}
