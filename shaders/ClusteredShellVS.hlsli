#include "ClusteredShellCommon.hlsli"
#include "Light.hlsli"

Interpolants main(uint index : SV_VertexID, uint instanceId : SV_InstanceID)
{
    CameraConstants cameraConstants = BufferLoad < CameraConstants > (rootConstants.cameraConstantsOffset);

    #ifdef POINT_LIGHT
    Light light = BufferLoad<Light>(rootConstants.lightsBufferOffset, instanceId);
    #endif

    #ifdef SHADOWED_POINT_LIGHT
    ShadowedLight light = BufferLoad<ShadowedLight>(rootConstants.lightsBufferOffset, instanceId);
    #endif
    
    float3x3 scaleMatrix = 0;
    scaleMatrix._11_22_33 = light.fadeEnd;
    float3 positionWS = BufferLoad<float3>(rootConstants.vertexBuffersOffset, index);
    positionWS = mul(positionWS, scaleMatrix);
    positionWS += light.position;

    Interpolants output;
    output.positionVS = mul(float4(positionWS, 1.0), cameraConstants.viewMatrix).xyz;
    output.position = mul(float4(positionWS, 1.0), cameraConstants.viewProjectionMatrix);
    output.renderTargetArrayIndex = instanceId;
    return output;
}
