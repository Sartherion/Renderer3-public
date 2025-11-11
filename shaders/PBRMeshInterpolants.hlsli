#pragma once

struct MeshInterpolants
{
#ifndef SHADOW_CASTER
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
    float3 positionWS : POSITIONWS;
    #ifdef DEBUG_PER_PIXEL_FACE_NORMAL
    nointerpolation float3 positionWS2 : POSITIONWS2;
    #endif
#endif
    float4 position : SV_Position;
};
    