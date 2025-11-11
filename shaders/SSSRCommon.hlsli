#include "Common.hlsli"
#include "ShadingHelpers.hlsli"

//#define USE_HALF_RESOLUTION_RAY_HIT_BUFFER
const static float minimumRoughness = 0.0001;
const static SSSRSettings settings = frameConstants.settings.sssrSettings;
const static float pdfScalingFactor = 1e7;

#ifndef USE_HALF_RESOLUTION_RAY_HIT_BUFFER 
const static TextureDimensions rayHitBufferDimensions = frameConstants.mainRenderTargetDimensions;
const static TextureDimensions specularBufferDimensions = frameConstants.mainRenderTargetDimensions;
#endif

//helper functions
//@note: taken from ch. 49 of ray tracing gems ii
float ComputeParallax(float3 X, float3 Xprev, float3 cameraDelta)
{
float3 V = normalize(X);
float3 Vprev = normalize(Xprev - cameraDelta);
    float cosa = saturate(dot(V, Vprev));
    float parallax = sqrt(1.0 - cosa * cosa) / max(cosa, 1e-6);
    //@note: left out normalization to 60 fps
    
    return parallax;
}
//@note: adapted from https://willpgfx.com/2015/07/screen-space-glossy-reflections/
#define CNST_MAX_SPECULAR_EXP 64
float RoughnessToSpecularPower(float r)
{
  return 2 / (pow(r,4)) - 2;
}

float SpecularPowerToConeAngle(float specularPower)
{
    // based on phong distribution model
    if(specularPower >= exp2(CNST_MAX_SPECULAR_EXP))
    {
        return 0.0f;
    }
    const float xi = 0.244f;
    float exponent = 1.0f / (specularPower + 1.0f);
    return acos(pow(xi, exponent));
}

float IsoscelesTriangleOpposite(float adjacentLength, float coneTheta)
{
    // simple trig and algebra - soh, cah, toa - tan(theta) = opp/adj, opp = tan(theta) * adj, then multiply * 2.0f for isosceles triangle base
    return 2.0f * tan(coneTheta) * adjacentLength;
}

float IsoscelesTriangleInRadius(float a, float h)
{
    float a2 = a * a;
    float fh2 = 4.0f * h * h;
    return (a * (sqrt(a2 + fh2) - a)) / (4.0f * h);
}

float IsoscelesTriangleNextAdjacent(float adjacentLength, float incircleRadius)
{
    // subtract the diameter of the incircle to get the adjacent side of the next level on the cone
    return adjacentLength - (incircleRadius * 2.0f);
}

struct RayMarchSettings
{
    int startMip;
    int2 startMipSize;
    float2 startMipTexelSize;
    int maxMip;
    int maxSteps;
    bool enableMarchBehind;
};

struct RayMarchHit
{
    float3 hitSS;
    float3 hitWS;
    float3 raySS;
    float3 rayWS;
};

//@note: based on Ulduag Gpu Pro 5 and AMD FFX sssr
float3 HierarchicalRayMarch(Texture2D<float2> depthPyramide, float3 originSS, float3 directionSS, RayMarchSettings settings)
{
    const float2 roundOffset = select(directionSS.xy > 0.0, 1.0, 0.0);
    float currentMip = settings.startMip;
    float2 currentMipSize = settings.startMipSize;
    float2 currentMipTexelSize = settings.startMipTexelSize;

    const float2 uvEpsilon = select(directionSS.xy < 0, -0.005, 0.005) * currentMipTexelSize;

    const float3 inverseDirectionScreenUv = rcp(directionSS);

    //initial advance to first intersection with cell boundary
    float3 cellBoundaryPlanes;
    cellBoundaryPlanes.xy = floor(originSS.xy * currentMipSize) + roundOffset;
    cellBoundaryPlanes.xy = cellBoundaryPlanes.xy * currentMipTexelSize.xy + uvEpsilon;

    float3 tCandidates = inverseDirectionScreenUv * (cellBoundaryPlanes - originSS);
    float t = min(tCandidates.x, tCandidates.y);

    float3 rayHitSS = originSS + t * directionSS;

    uint stepsCount = 0;
    while (currentMip >= settings.startMip && stepsCount < settings.maxSteps) //@todo: exit due to low occupancy
    {
        #if 0
        if (!IsWithinTextureBorders(rayHitSS.xy))
        {
            return 0.0;
        }
        #endif

        const float2 pixelPosition = rayHitSS.xy * currentMipSize;
        const float depth = depthPyramide.Load(uint3(pixelPosition, currentMip)).r;

        bool behindZPlane;
        if (settings.enableMarchBehind)
        {
            float maxDepth = depthPyramide.Load(uint3(pixelPosition, currentMip)).g;
            behindZPlane = rayHitSS.z > depth && rayHitSS.z < maxDepth;
        }
        else
        {
            behindZPlane = rayHitSS.z > depth;
        }

        cellBoundaryPlanes.xy = floor(pixelPosition) + roundOffset;
        cellBoundaryPlanes.xy = cellBoundaryPlanes.xy * currentMipTexelSize.xy + uvEpsilon;
        cellBoundaryPlanes.z = depth;

        float3 tCandidates = inverseDirectionScreenUv * (cellBoundaryPlanes - originSS);
        float tMin = min(tCandidates.x, tCandidates.y);

        bool zPlaneHit = false;

        tCandidates.z = directionSS.z > 0 ? tCandidates.z : FLT_MAX;
        if (tCandidates.z < tMin)
        {
            tMin = tCandidates.z;
            zPlaneHit = true;
        }
            
        t = behindZPlane ? t : tMin;
        if (behindZPlane || zPlaneHit)
        {
            currentMip--;
            currentMipSize *= 2.0;
            currentMipTexelSize *= 0.5;
        }
        else
        {
            if (currentMip < settings.maxMip)
            {
                currentMip++;
                currentMipSize *= 0.5;
                currentMipTexelSize *= 2.0;
            }
        }
        rayHitSS = originSS + t * directionSS;
        stepsCount++;
    }

    if (stepsCount >= settings.maxSteps)
    {
        return 0.0;
    }
    
    if (!IsWithinTextureBorders(rayHitSS.xy))
    {
        return 0.0;
    }

    return rayHitSS;
}
