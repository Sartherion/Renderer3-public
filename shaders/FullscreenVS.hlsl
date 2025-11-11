#include "FullscreenCommon.hlsli"

Interpolants main(uint vertexId : SV_VertexID)
{
    Interpolants output;

    switch (vertexId)
    {
        case 0:
            output.position = float4(-1.0f, 1.0f, 1.0f, 1.0f);
            output.texCoord = float2(0.0f, 0.0f);
            break;
        case 1:
            output.position = float4(3.0f, 1.0f, 1.0f, 1.0f);
            output.texCoord = float2(2.0f, 0.0f);
            break;
        default:
            output.position = float4(-1.0f, -3.0f, 1.0f, 1.0f);
            output.texCoord = float2(0.0f, 2.0f);
            break;
    }

    return output;
}