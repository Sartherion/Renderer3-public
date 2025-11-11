#include "CubeMapCommon.hlsli"
#include "SkyboxCommon.hlsli"
#include "PBRCommon.hlsli"
#include "GlobalBuffer.hlsli"

#include "DebugHelpers.hlsli"


struct Constants
{
    float4 position;
    float4x4 viewProjectionMatrix;
};

Interpolants main(float3 position : POSITION) 
{
    Interpolants output;
    Constants constants = BufferLoad<Constants>(renderIds.cubeMapConstantsOffset);
    output.positionOS = position;
    output.position = float4(position, 1.0f);
    output.position += constants.position;
    output.position = mul(output.position, constants.viewProjectionMatrix);
    output.position = output.position.xyww; // setting z=w means depth will be always 1.0f
    return output;
}