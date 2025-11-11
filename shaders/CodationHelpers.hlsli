#pragma once
#include "MathHelpers.hlsli"

//Octahedral encoding for normals, taken from https://knarkowicz.wordpress.com/2014/04/16/octahedron-normal-vector-encoding/
float2 OctWrap(float2 v)
{
    return (1.0 - abs(v.yx)) * select(v.xy >= 0.0, 1.0, -1.0);
}
 
float2 OctahedralEncode(float3 n)
{
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    n.xy = n.z >= 0.0 ? n.xy : OctWrap(n.xy);
    n.xy = n.xy * 0.5 + 0.5;
    return n.xy;
}
 
float3 OctahedralDecode(float2 f)
{
    f = f * 2.0 - 1.0;
 
    // https://twitter.com/Stubbesaurus/status/937994790553227264
    float3 n = float3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
    float t = saturate(-n.z);
    n.xy += select(n.xy >= 0.0, -t, t);

    return normalize(n);
}

float3 GetCubeMapDirection(float2 faceCoordinates, int index) //face coordinates from -1 to 1
{
    switch (index)
    {
        case 0:
            return float3(1, -faceCoordinates.y, -faceCoordinates.x);
        case 1:
            return float3(-1, -faceCoordinates.y, faceCoordinates.x);
        case 2:
            return float3(faceCoordinates.x, 1, faceCoordinates.y);
        case 3:
            return float3(faceCoordinates.x, -1, -faceCoordinates.y);
        case 4:
            return float3(faceCoordinates.x, -faceCoordinates.y, 1);
        case 5:
            return float3(-faceCoordinates.x, -faceCoordinates.y, -1);
    }
    return 0;
}

//@note: based on https://www.gamedev.net/forums/topic/692761-manual-cubemap-lookupfiltering-in-hlsl/
void GetCubeMapUvAndFaceId(float3 direction, out float2 uv, out uint faceId)
{
    float maxComponent = MaximumComponent(abs(direction)); 

    uv = float2(direction.y, direction.z);

    if(direction.x == maxComponent)
    {
        faceId = 0;
        uv = float2(-direction.z, -direction.y) / direction.x;
    }
    else if(-direction.x == maxComponent)
    {
        faceId = 1;
        uv = float2(direction.z, -direction.y) / -direction.x;
    }
    else if(direction.y == maxComponent)
    {
        faceId = 2;
        uv = float2(direction.x, direction.z) / direction.y;
    }
    else if(-direction.y == maxComponent)
    {
        faceId = 3;
        uv = float2(direction.x, -direction.z) / -direction.y;
    }
    else if(direction.z == maxComponent)
    {
        faceId = 4;
        uv = float2(direction.x, -direction.y) / direction.z;
    }
    else /*if(-direction.z == maxComponent)*/
    {
        faceId = 5;
        uv = float2(-direction.x, -direction.y) / -direction.z;
    }
}

float3 GetCubeMapFaceNormal(uint faceId)
{
    static float3 faceNormals[] =
    {
        float3(1.0, 0.0, 0.0),
        float3(-1.0, 0.0, 0.0),
        float3(0.0, 1.0, 0.0),
        float3(0.0, -1.0, 0.0),
        float3(0.0, 0.0, 1.0),
        float3(0.0, 0.0, -1.0),
    };
    
    return faceNormals[faceId];
}

float3 GetCubeMapFaceNormal(float3 direction)
{
    float2 unused;
    uint faceId;
    GetCubeMapUvAndFaceId(direction, unused, faceId);

    return GetCubeMapFaceNormal(faceId);
}
