#include "ClusteredShellCommon.hlsli"
#include "ViewHelpers.hlsli"

//@note: computed normal always has negative y component, i.e. points from top to bottom
float3 ComputeVerticalFrustumPlanNormal(float3 edge)
{
    return normalize(float3(0.0, -1.0f, edge.y / edge.z)); 
}

//@note: computed normal always has positive x component, i.e. points from left to right
float3 ComputeHorizontalFrustumPlaneNormal(float3 edge)
{
    return normalize(float3(1.0, 0.0f, -edge.x / edge.z));
}

//Taken from ¨Ortegren and Persson
bool LinesegmentPlaneIntersection(float3 p0, float3 p1, float3 pn, out float t)
{
	float3 u = p1 - p0;

	float D = dot(pn, u);
	float N = -dot(pn, p0);

	t = N / D;
	return !(t != saturate(t)); //@note: double negation in order to also work in the case of NaN
}

bool RayTriangleInterssection(float3 rayDirection, float3 vertex0, float3 vertex1, float3 vertex2, out float posZ)
{
    posZ = 0; // to silence compiler warning
	float3 edge1 = vertex1 - vertex0;
	float3 edge2 = vertex2 - vertex0;
	float3 q = cross(rayDirection, edge2);
	float a = dot(edge1, q);

	if (a > -0.000001 && a < 0.000001)
		return false;

	float f = 1.0 / a;
	float u = f * dot(-vertex0, q);

	if (u != saturate(u))
		return false;

	float3 r = cross(-vertex0, edge1);
	float v = f * dot(rayDirection, r);

	if (v < 0.0 || (u + v) > 1.0)
		return false;

	posZ = f * dot(edge2, r) * rayDirection.z;

	 return true;
}

bool IsInXSlice(float3 normalTop, float3 normalBottom, float3 p)
{
	return normalTop.y * p.y + normalTop.z * p.z >= 0.0
		&& normalBottom.y * p.y + normalBottom.z * p.z >= 0.0;
}

bool IsInYSlice(float3 normalLeft, float3 normalRight, float3 p)
{
	return normalLeft.x * p.x + normalLeft.z * p.z >= 0.0
		&& normalRight.x * p.x + normalRight.z * p.z >= 0.0;
}

void UpdateDepths(float depth, inout float minDepth, inout float maxDepth)
{
        minDepth = min(minDepth, depth);
        maxDepth = max(maxDepth, depth);
}

void UpdateDepthLinePlaneIntersection(float3 p0, float3 p1, float3 pn, inout float minDepth, inout float maxDepth)
{
    float t;
    if (LinesegmentPlaneIntersection(p0, p1, pn, t))
    {
        float depth = lerp(p0, p1, t).z;
        UpdateDepths(depth, minDepth, maxDepth);
    }
}

float2 main(Interpolants input, bool isFrontFace : SV_IsFrontFace) : SV_TARGET
{
    CameraConstants cameraConstants = BufferLoad < CameraConstants > (rootConstants.cameraConstantsOffset);

    const uint2 clusterCountXY = uint2(rootConstants.clusterCountX, rootConstants.clusterCountY);
    const float2 texelSize = 1.0 / clusterCountXY;
    const float aspectRatio = clusterCountXY.x * texelSize.y;
    const float focalLength = cameraConstants.projectionMatrix._m11;

    const float2 screenSpacePosition = input.position.xy;
    const float2 uvSubFrustumTopLeft = (screenSpacePosition + float2(-0.5, -0.5)) * texelSize;
    const float2 uvSubFrustumTopRight = (screenSpacePosition + float2(0.5, -0.5)) * texelSize;
    const float2 uvSubFrustumBottomLeft = (screenSpacePosition + float2(-0.5, 0.5)) * texelSize;
    const float2 uvSubFrustumBottomRight = (screenSpacePosition + float2(0.5, 0.5)) * texelSize;
    const float3 subFrustumTopLeftVS = SSToVS(uvSubFrustumTopLeft, aspectRatio, focalLength);
    const float3 subFrustumTopRightVS = SSToVS(uvSubFrustumTopRight, aspectRatio, focalLength);
    const float3 subFrustumBottomLeftVS = SSToVS(uvSubFrustumBottomLeft, aspectRatio, focalLength);
    const float3 subFrustumBottomRightVS = SSToVS(uvSubFrustumBottomRight, aspectRatio, focalLength);

    const float3 normalLeft = ComputeHorizontalFrustumPlaneNormal(subFrustumTopLeftVS);
    const float3 normalRight = -ComputeHorizontalFrustumPlaneNormal(subFrustumBottomRightVS);
    const float3 normalTop = ComputeVerticalFrustumPlanNormal(subFrustumTopLeftVS);
    const float3 normalBottom = -ComputeVerticalFrustumPlanNormal(subFrustumBottomRightVS);

	const float3 vertex0 = GetAttributeAtVertex(input.positionVS, 0);
	const float3 vertex1 = GetAttributeAtVertex(input.positionVS, 1);
	const float3 vertex2 = GetAttributeAtVertex(input.positionVS, 2);


	float minDepth = FLT_MAX; 
	float maxDepth = -FLT_MAX;

	//Case a
	UpdateDepthLinePlaneIntersection(vertex0, vertex1, normalLeft, minDepth, maxDepth);
	UpdateDepthLinePlaneIntersection(vertex0, vertex2, normalLeft, minDepth, maxDepth);
	UpdateDepthLinePlaneIntersection(vertex1, vertex2, normalLeft, minDepth, maxDepth);

	UpdateDepthLinePlaneIntersection(vertex0, vertex1, normalRight, minDepth, maxDepth);
	UpdateDepthLinePlaneIntersection(vertex0, vertex2, normalRight, minDepth, maxDepth);
	UpdateDepthLinePlaneIntersection(vertex1, vertex2, normalRight, minDepth, maxDepth);

	UpdateDepthLinePlaneIntersection(vertex0, vertex1, normalTop, minDepth, maxDepth);
	UpdateDepthLinePlaneIntersection(vertex0, vertex2, normalTop, minDepth, maxDepth);
	UpdateDepthLinePlaneIntersection(vertex1, vertex2, normalTop, minDepth, maxDepth);

	UpdateDepthLinePlaneIntersection(vertex0, vertex1, normalBottom, minDepth, maxDepth);
	UpdateDepthLinePlaneIntersection(vertex0, vertex2, normalBottom, minDepth, maxDepth);
	UpdateDepthLinePlaneIntersection(vertex1, vertex2, normalBottom, minDepth, maxDepth);

	//Case b
    float depth;
    if (RayTriangleInterssection(subFrustumTopLeftVS, vertex0, vertex1, vertex2, depth))
    {
        UpdateDepths(depth, minDepth, maxDepth);
    }

    if (RayTriangleInterssection(subFrustumTopRightVS, vertex0, vertex1, vertex2, depth))
    {
        UpdateDepths(depth, minDepth, maxDepth);
    }

    if (RayTriangleInterssection(subFrustumBottomLeftVS, vertex0, vertex1, vertex2, depth))
    {
        UpdateDepths(depth, minDepth, maxDepth);
    }

    if (RayTriangleInterssection(subFrustumBottomRightVS, vertex0, vertex1, vertex2, depth))
    {
        UpdateDepths(depth, minDepth, maxDepth);
    }

	//Case c
	if (IsInXSlice(normalTop, normalBottom, vertex0) && IsInYSlice(normalLeft, normalRight, vertex0))
	{
        UpdateDepths(vertex0.z, minDepth, maxDepth);
	}

	if (IsInXSlice(normalTop, normalBottom, vertex1) && IsInYSlice(normalLeft, normalRight, vertex1))
	{
        UpdateDepths(vertex1.z, minDepth, maxDepth);
	}

	if (IsInXSlice(normalTop, normalBottom, vertex2) && IsInYSlice(normalLeft, normalRight, vertex2))
	{
        UpdateDepths(vertex2.z, minDepth, maxDepth);
	}


    const float nearZ = cameraConstants.frustumData.nearZ;
    const float farZ = cameraConstants.frustumData.farZ;
    
    if (isFrontFace)
    {
        float normalizedMinDepth = saturate(NormalizeLinearDepth(minDepth, nearZ, farZ));
        uint minDepthSlice = ZSliceDistributionLinear(normalizedMinDepth, clusterCountZ);
        return float2(minDepthSlice / 255.0, 1.f);
    }
    else
    {
        float normalizedMaxDepth = NormalizeLinearDepth(maxDepth, nearZ, farZ);
        uint maxDepthSlice = ZSliceDistributionLinear(normalizedMaxDepth, clusterCountZ);
        return float2(1.0, (clusterCountZ - 1 - maxDepthSlice) / 255.0);
    }
}

