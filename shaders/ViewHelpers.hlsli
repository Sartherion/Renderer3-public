#pragma once
#include "MathHelpers.hlsli"

float3 CalculateWSCameraRay(float2 positionNDC, float4x4 inverseView, float aspectRatio, float focalLength)
{
    float4 rayDirectionVS = float4(positionNDC.x * aspectRatio, positionNDC.y, focalLength, 0.0);
    return mul(rayDirectionVS, inverseView).xyz;
}

float NonlinearToLinearDepth(float nonlinearDepth, float4x4 projection)
{
    return projection[3][2] / (nonlinearDepth - projection[2][2]);
}

float LinearToNonlinearDepth(float linearDepth, float4x4 projection)
{
    return projection[2][2] + projection[3][2] / linearDepth;
}

float LinearToNonlinearDepth(float linearDepth, float nearZ, float farZ)
{
	const float a = farZ / (farZ - nearZ);
	const float b = -a * nearZ;
    return  a + b / linearDepth;
}

float NonlinearToLinearDepth(float nonlinearDepth, float nearZ, float farZ)
{
	const float a = farZ / (farZ - nearZ);
	const float b = -a * nearZ;
    return b / (nonlinearDepth - a);
}

float2 SSToNDC(float2 screenUv)
{
    return float2(2 * screenUv.x - 1, -2 * screenUv.y + 1);
}

float2 NDCToSS(float2 ndc)
{
    return float2(0.5 * (ndc.x + 1), -0.5 * (ndc.y - 1));
}

float3 SSToVS(float2 screenUv, float aspectRatio, float focalLength) 
{
    float3 positionVS;
    screenUv = SSToNDC(screenUv);
    positionVS.x = screenUv.x * aspectRatio;
    positionVS.y = screenUv.y;
    positionVS.z = focalLength;
    return positionVS;
}

float3 SSToWS(float2 uv, float nonlinearDepth, float4x4 inverseViewProjectionMatrix)
{
    float4 positionNDC = float4(SSToNDC(uv), nonlinearDepth, 1.0);
    positionNDC = mul(positionNDC, inverseViewProjectionMatrix); 
    positionNDC /= positionNDC.w;
    return positionNDC.xyz;
}

float3 WSToSS(float3 positionWS, float4x4 viewProjectionMatrix)
{
    float4 positionNDC = mul(float4(positionWS, 1.0), viewProjectionMatrix);
    positionNDC/= positionNDC.w;
    return float3(NDCToSS(positionNDC.xy), positionNDC.z);
}

//@note: cf. https://stackoverflow.com/questions/21648630/radius-of-projected-sphere-in-screen-space
float ProjectSphereRadius(float radius, float depth, float4x4 projectionMatrix)
{
    float c = sqrt(max(Square(depth) - Square(radius), 0.0001));
    return projectionMatrix[0][0] * radius / c;
}

bool IsWithinNDCFrustum(float3 positionNDC)
{
    return all(positionNDC >= -1.0) && all(positionNDC <= 1.0);
}

bool IsWithinTextureBorders(float2 uv)
{
    return all(uv == saturate(uv));
}

// assumes symmetric frustum; Normals assumed to point to the inside the frustum
bool IsWithinFrustum(float4 sphereVS, float3 normalTopVS, float3 normalLeftVS, float nearZ, float farZ)
{
    float3 center = sphereVS.xyz;
    float radius = sphereVS.w;

    float3 normalBottomVS = float3(normalTopVS.x, -normalTopVS.y, normalTopVS.z); 
    float3 normalRightVS = float3(-normalLeftVS.x, normalLeftVS.y, normalLeftVS.z);

    return dot(center, normalTopVS) >= -radius
        && dot(center, normalBottomVS) >= -radius
        && dot(center, normalLeftVS) >= -radius
        && dot(center, normalRightVS) >= -radius
        && center.z + radius >= nearZ
        && center.z - radius <= farZ;
}
