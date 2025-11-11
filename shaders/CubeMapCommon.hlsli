#pragma once
#define CUBE_MAP

#include "GlobalBuffer.hlsli"

struct CubeMapTransform
{
    float4 position;
};

static const float3 x = float3(1.0f, 0.0f, 0.0f), y = float3(0.0f, 1.0f, 0.0f), z = float3(0.0f, 0.0f, 1.0f);
static const float3 bases[6][3] = //@todo: might actually not be rotated correctly for cubemap sampling?
{
    { -z, y, x },
    { z, y, -x },
    { x, -z, y },
    { x, z, -y },
    { x, y, z },
    { -x, y, -z }
};

static const float PI = 3.14159265358979323846f;
static const float far = 1000.0f;
static const float near = 0.1f; //@todo: these might actually need to be configurable per cube map?
static const float a = 1.0f /*1.0f / tan(0.25 * PI)*/; //is one
static const float b = far / (far - near);
static const float c = -far * near / (far - near);

float4x4 BuildViewMatrix(int direction, float3 position)
{
    float4x4 viewMatrix;
    const float3 u = bases[direction][0];
    const float3 v = bases[direction][1];
    const float3 w = bases[direction][2];
    const float Qu = dot(position, u);
    const float Qv = dot(position, v);
    const float Qw = dot(position, w);
    
    viewMatrix._m00_m10_m20_m30 = float4(u, -Qu);
    viewMatrix._m01_m11_m21_m31 = float4(v, -Qv);
    viewMatrix._m02_m12_m22_m32 = float4(w, -Qw);
    viewMatrix._m03_m13_m23_m33 = float4(0.0f, 0.0f, 0.0f, 1.0f);
    
    return viewMatrix;
}

float4x4 BuildProjectionMatrix()
{
    float4x4 projectionMatrix = 0.0f; //@todo: does this set all elements to 0?
    
    projectionMatrix._m00_m11 = a; //@todo: does this set both to a?
    projectionMatrix._m22 = b;
    projectionMatrix._m23 = 1.0f;
    projectionMatrix._m32 = c;

    return projectionMatrix;
}



float4x4 CalculateCubeMapViewProjectionMatrix(uint arrayIndex, uint constantsId)
{
    float4x4 viewProjectionMatrix;

    const uint faceIndex = arrayIndex % 6;
    const uint transformIndex = arrayIndex / 6;
    
    CubeMapTransform transform = BufferLoad<CubeMapTransform>(constantsId, transformIndex); //@todo: abuse of constant
    //StructuredBuffer<CubeMapTransform> cubeMapTransforms = ResourceDescriptorHeap[constantsId];
    
    //CubeMapTransform transform = cubeMapTransforms[transformIndex]; //@todo: use slot?
    viewProjectionMatrix = mul(BuildViewMatrix(faceIndex, transform.position.xyz), BuildProjectionMatrix());
    
    return viewProjectionMatrix;
}
