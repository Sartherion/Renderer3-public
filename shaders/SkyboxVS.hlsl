#include "SkyboxCommon.hlsli"

Interpolants main(uint index : SV_VertexID) 
{
    float3 position = BufferLoad<float3>(rootConstants.vertexBuffersOffset, index);
    CameraConstants cameraConstants = BufferLoad < CameraConstants > (rootConstants.cameraConstantsOffset);

    Interpolants output;
    output.positionOS = position;
    output.position = float4(position, 1.0f);
    output.position.xyz += cameraConstants.position.xyz;
    output.position = mul(output.position, cameraConstants.viewProjectionMatrix);
    output.position = output.position.xyww; // setting z=w means depth will be always 1.0f
    return output;
}