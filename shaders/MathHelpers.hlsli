#pragma once

static const float FLT_MAX = 3.402823466e+38;
static const float PI = 3.14159265f;

static const float4x4 IdentityMatrix =
{
	{ 1, 0, 0, 0 },
	{ 0, 1, 0, 0 },
	{ 0, 0, 1, 0 },
	{ 0, 0, 0, 1 }
};

//@note: based on https://sakibsaikia.github.io/graphics/2022/01/04/Nan-Checks-In-HLSL.html
template <typename floatN>
bool IsNaN(floatN x)
{
    return any((asuint(x) & 0x7fffffff) > 0x7f800000);
}

template<typename T>
T Square(T value)
{
    return value * value;
}

float LengthSquared(float3 a)
{
    return dot(a, a);
}

float DistanceSquared(float3 a, float3 b)
{
    return LengthSquared(a - b);
}

template <typename floatN>
float ComponentSum(floatN v)
{
    return dot(1, v);
}

float MaximumComponent(float2 value)
{
    return max(value.r, value.g);
}

float MaximumComponent(float3 value)
{
    return max(MaximumComponent(value.rg), value.b);
}

float MaximumComponent(float4 value)
{
    return max(MaximumComponent(value.rgb), value.a);
}

float MinimumComponent(float2 value)
{
    return min(value.r, value.g);
}

float MinimumComponent(float3 value)
{
    return min(MinimumComponent(value.rg), value.b);
}

float MinimumComponent(float4 value)
{
    return min(MinimumComponent(value.rgb), value.a);
}

float3 PerpendicularVector(float3 unitVector)
{
    float3 perpendicularUnitVector;
    if (abs(unitVector.z) > 0.0)
    {
        float k = sqrt(unitVector.y * unitVector.y + unitVector.z * unitVector.z);
        perpendicularUnitVector.x = 0.0;
        perpendicularUnitVector.y = -unitVector.z / k;
        perpendicularUnitVector.z = unitVector.y / k;
    }
    else
    {
        float k = sqrt(unitVector.x * unitVector.x + unitVector.y * unitVector.y);
        perpendicularUnitVector.x = unitVector.y / k;
        perpendicularUnitVector.y = -unitVector.x / k;
        perpendicularUnitVector.z = 0.0;
    }
    
    return perpendicularUnitVector;
}

//both inputs are assumed to be unit vectors
float4x4 Rotation(float3 direction, float3 up)
{
    float3 row0 = up;
    float3 row1 = cross(direction, up);
    float3 row2 = direction;

    return float4x4(float4(row0, 0), float4(row1, 0), float4(row2, 0), float4(0, 0, 0, 1));
}

float4x4 Rotation(float3 unitVector)
{
    float3 up = PerpendicularVector(unitVector);
    return Rotation(unitVector, up);
}

float4 CosineSH4()
{
    float a0 = PI;
    float3 a1 = 2.0 * PI / 3.0;

    return float4(a0, a1);
}

float4 EvaluateSH4Basis(float3 direction)
{
    //@note: harmonics from An Efficient Representation for Irradiance Environment Maps
    float y0 = 0.282095; // Y00

    float a = 0.488603;
    float3 y1 = a * direction.yzx; //order: Y1-1, Y10, Y11
    return float4(y0, y1);
}

//@note: taken  from http://www.thetenthplanet.de/archives/1180
float3x3 CalculateCotangentFrame(float3 N, float3 viewVector, float2 uv)
{
    float3 dp1 = ddx(viewVector);
    float3 dp2 = ddy(viewVector);
    float2 duv1 = ddx(uv);
    float2 duv2 = ddy(uv);
    
    float3 dp2perp = cross(dp2, N);
    float3 dp1perp = cross(N, dp1);
    float3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    float3 B = dp2perp * duv1.y + dp1perp * duv2.y;
    
    float invmax = rsqrt(max(dot(T, T), dot(B, B)));
    return float3x3(T * invmax, B * invmax, N);
}

float3 PerturbNormal(float3 normalMapSample, float3x3 TBN)
{
    normalMapSample = normalMapSample * 255. / 127. - 128. / 127.;
    normalMapSample.z = sqrt(1. - dot(normalMapSample.xy, normalMapSample.xy));
    //normalMapSample.y = -normalMapSample.y;
    
    return normalize(mul(normalMapSample, TBN));
}

struct BoundingBox
{
    float3 center;
    float3 extents;
};

bool IsInsideBoundingBox(float3 position, BoundingBox boundingBox)
{
    float3 v = position - boundingBox.center;
    float3 bounds = boundingBox.extents;
    return (((v.x <= bounds.x && v.x >= -bounds.x) &&
        (v.y <= bounds.y && v.y >= -bounds.y) &&
        (v.z <= bounds.z && v.z >= -bounds.z)) != 0);
}

template <typename T = float4>
struct WeightedSum
{
    T valueSum;
    float weightSum;

    static WeightedSum Init()
    {
        WeightedSum result;
        result.valueSum = 0;
        result.weightSum = 0;

        return result;
    }

    void AddValue(T value, float weight)
    {
        weightSum += weight;
        valueSum += weight * value;
    }

    T GetNormalizedSum()
    {
        return valueSum / weightSum;
    }
};
