#pragma once 
#include "MathHelpers.hlsli"

uint2 GetSampleOrigin(float2 uv, uint2 size)
{
    return floor(uv * size - 0.5);
}

//@note: adapted from 'Fast Denoising with Self Stabilizing Recurrent Blurs' presentation
struct BilinearFilter
{
    float2 origin;
    float2 weights;
};

BilinearFilter GetBilinearFilter(float2 uv, float2 texSize)
{
    BilinearFilter result;
    result.origin = GetSampleOrigin(uv, texSize);
    result.weights = frac(uv * texSize - 0.5);
    return result;
}

float4 GetBilinearCustomWeights(BilinearFilter f, float4 customWeights)
{
    float4 weights;
    weights.x = (1.0 - f.weights.x) * (1.0 - f.weights.y);
    weights.y = f.weights.x * (1.0 - f.weights.y);
    weights.z = (1.0 - f.weights.x) * f.weights.y;
    weights.w = f.weights.x * f.weights.y;
    return weights * customWeights;
}

template <typename floatN>
floatN ApplyBilinearCustomWeights(floatN s00, floatN s10, floatN s01, floatN s11, float4 w, bool normalize = true)
{
    floatN r = s00 * w.x + s10 * w.y + s01 * w.z + s11 * w.w;
    if (any(w != 0))
    {
        return r * (normalize ? rcp(dot(w, 1.0)) : 1.0);
    }
    else
    {
        return 0;
    }
}

float GetGeometryWeight(float3 sampleVector, float3 n0, float planeDistNorm )
{
// where planeDistNorm = accumSpeedFactor / ( 1.0 + centerZ );
// It represents { 1 / "max possible allowed distance between a point and the plane" }
float distToPlane = dot( n0, sampleVector);
float w = saturate( 1.0 - abs( distToPlane ) * planeDistNorm );
return w;
}

template<typename T>
T SampleWithCustomBilinearWeights(Texture2DArray<T> texture, int2 originPixelPosition, float4 weights, int arrayIndex = 0, int mipLevel = 0)
{
    T s00 = texture.Load(int4(originPixelPosition + int2(0, 0), arrayIndex, mipLevel)); 
    T s10 = texture.Load(int4(originPixelPosition + int2(1, 0), arrayIndex, mipLevel));
    T s01 = texture.Load(int4(originPixelPosition + int2(0, 1), arrayIndex, mipLevel));
    T s11 = texture.Load(int4(originPixelPosition + int2(1, 1), arrayIndex, mipLevel));

    return ApplyBilinearCustomWeights(s00, s10, s01, s11, weights);
}

enum class FilterType
{
    Box,
    Triangle,
    Gaussian,
    BlackmanHarris,
    Smoothstep,
    BSpline,
    CatmullRom,
    Mitchell,
    GeneralizedCubic,
    Sinc,
};

static const float Pi = 3.14159265f;
//@note: adapted from https://gist.github.com/TheRealMJP/c83b8c0f46b63f3a88a5986f4fa982b1
//=================================================================================================
//
//  MSAA Filtering 2.0 Sample
//  by MJP
//  http://mynameismjp.wordpress.com/
//
//  All code licensed under the MIT license
//
//=================================================================================================

// All filtering functions assume that 'x' is normalized to [0, 1], where 1 == FilteRadius
float FilterBox(in float x)
{
    return x <= 1.0f;
}

static float FilterTriangle(in float x)
{
    return saturate(1.0f - x);
}

static float FilterGaussian(in float x, float sigma)
{
    const float g = 1.0f / sqrt(2.0f * 3.14159f * sigma * sigma);
    return (g * exp(-(x * x) / (2 * sigma * sigma)));
}

 float FilterCubic(in float x, in float B, in float C)
{
    float y = 0.0f;
    float x2 = x * x;
    float x3 = x * x * x;
    if(x < 1)
        y = (12 - 9 * B - 6 * C) * x3 + (-18 + 12 * B + 6 * C) * x2 + (6 - 2 * B);
    else if (x <= 2)
        y = (-B - 6 * C) * x3 + (6 * B + 30 * C) * x2 + (-12 * B - 48 * C) * x + (8 * B + 24 * C);

    return y / 6.0f;
}

float FilterSinc(in float x, in float filterRadius)
{
    float s;

    x *= filterRadius * 2.0f;

    if(x < 0.001f)
        s = 1.0f;
    else
        s = sin(x * Pi) / (x * Pi);

    return s;
}

float FilterBlackmanHarris(in float x)
{
    x = 1.0f - x;

    const float a0 = 0.35875f;
    const float a1 = 0.48829f;
    const float a2 = 0.14128f;
    const float a3 = 0.01168f;
    return saturate(a0 - a1 * cos(Pi * x) + a2 * cos(2 * Pi * x) - a3 * cos(3 * Pi * x));
}

float FilterSmoothstep(in float x)
{
    return 1.0f - smoothstep(0.0f, 1.0f, x);
}

float Filter(in float x, in FilterType filterType, in float filterRadius = 1.0, in bool rescaleCubic = true)
{
    // Cubic filters naturually work in a [-2, 2] domain. For the resolve case we
    // want to rescale the filter so that it works in [-1, 1] instead
    float cubicX = rescaleCubic ? x * 2.0f : x;

    switch (filterType)
    {
        case FilterType::Box:
            return FilterBox(x);
        case FilterType::Triangle:
            return FilterTriangle(x);
        case FilterType::Gaussian:
            return FilterGaussian(x, filterRadius);
        case FilterType::BlackmanHarris:
            return FilterBlackmanHarris(x);
        case FilterType::Smoothstep:
            return FilterSmoothstep(x);
        case FilterType::BSpline:
            return FilterCubic(cubicX, 1.0, 0.0f);
        case FilterType::CatmullRom:
            return FilterCubic(cubicX, 0, 0.5f);
        case FilterType::Mitchell:
            return FilterCubic(cubicX, 1 / 3.0f, 1 / 3.0f);
        //case FilterType::GeneralizedCubic:
        //    return FilterCubic(cubicX, CubicB, CubicC);
        case FilterType::Sinc:
            return FilterSinc(x, filterRadius);
        default:
            return 1.0f;
    }
}


// From "Temporal Reprojection Anti-Aliasing"
// https://github.com/playdeadgames/temporal
float3 ClipAABB(float3 aabbMin, float3 aabbMax, float3 prevSample, float3 avg = 0)
{
    #if 1
        // note: only clips towards aabb center (but fast!)
        float3 p_clip = 0.5 * (aabbMax + aabbMin);
        float3 e_clip = 0.5 * (aabbMax - aabbMin);

        float3 v_clip = prevSample - p_clip;
        float3 v_unit = v_clip.xyz / e_clip;
        float3 a_unit = abs(v_unit);
        float ma_unit = max(a_unit.x, max(a_unit.y, a_unit.z));

        if (ma_unit > 1.0)
            return p_clip + v_clip / ma_unit;
        else
            return prevSample;// point inside aabb
    #else
        float3 r = prevSample - avg;
        float3 rmax = aabbMax - avg.xyz;
        float3 rmin = aabbMin - avg.xyz;

        const float eps = 0.000001f;

        if (r.x > rmax.x + eps)
            r *= (rmax.x / r.x);
        if (r.y > rmax.y + eps)
            r *= (rmax.y / r.y);
        if (r.z > rmax.z + eps)
            r *= (rmax.z / r.z);

        if (r.x < rmin.x - eps)
            r *= (rmin.x / r.x);
        if (r.y < rmin.y - eps)
            r *= (rmin.y / r.y);
        if (r.z < rmin.z - eps)
            r *= (rmin.z / r.z);

        return avg + r;
    #endif
}

// The following code is licensed under the MIT license: https://gist.github.com/TheRealMJP/bc503b0b87b643d3505d41eab8b332ae

// Samples a texture with Catmull-Rom filtering, using 9 texture fetches instead of 16.
// See http://vec3.ca/bicubic-filtering-in-fewer-taps/ for more details
float4 SampleTextureCatmullRom(in Texture2D<float4> tex, in SamplerState linearSampler, in float2 uv, in float2 texSize)
{
    // We're going to sample a a 4x4 grid of texels surrounding the target UV coordinate. We'll do this by rounding
    // down the sample location to get the exact center of our "starting" texel. The starting texel will be at
    // location [1, 1] in the grid, where [0, 0] is the top left corner.
    float2 samplePos = uv * texSize;
    float2 texPos1 = floor(samplePos - 0.5f) + 0.5f;

    // Compute the fractional offset from our starting texel to our original sample location, which we'll
    // feed into the Catmull-Rom spline function to get our filter weights.
    float2 f = samplePos - texPos1;

    // Compute the Catmull-Rom weights using the fractional offset that we calculated earlier.
    // These equations are pre-expanded based on our knowledge of where the texels will be located,
    // which lets us avoid having to evaluate a piece-wise function.
    float2 w0 = f * (-0.5f + f * (1.0f - 0.5f * f));
    float2 w1 = 1.0f + f * f * (-2.5f + 1.5f * f);
    float2 w2 = f * (0.5f + f * (2.0f - 1.5f * f));
    float2 w3 = f * f * (-0.5f + 0.5f * f);

    // Work out weighting factors and sampling offsets that will let us use bilinear filtering to
    // simultaneously evaluate the middle 2 samples from the 4x4 grid.
    float2 w12 = w1 + w2;
    float2 offset12 = w2 / (w1 + w2);

    // Compute the final UV coordinates we'll use for sampling the texture
    float2 texPos0 = texPos1 - 1;
    float2 texPos3 = texPos1 + 2;
    float2 texPos12 = texPos1 + offset12;

    texPos0 /= texSize;
    texPos3 /= texSize;
    texPos12 /= texSize;

    float4 result = 0.0f;
    result += tex.SampleLevel(linearSampler, float2(texPos0.x, texPos0.y), 0.0f) * w0.x * w0.y;
    result += tex.SampleLevel(linearSampler, float2(texPos12.x, texPos0.y), 0.0f) * w12.x * w0.y;
    result += tex.SampleLevel(linearSampler, float2(texPos3.x, texPos0.y), 0.0f) * w3.x * w0.y;

    result += tex.SampleLevel(linearSampler, float2(texPos0.x, texPos12.y), 0.0f) * w0.x * w12.y;
    result += tex.SampleLevel(linearSampler, float2(texPos12.x, texPos12.y), 0.0f) * w12.x * w12.y;
    result += tex.SampleLevel(linearSampler, float2(texPos3.x, texPos12.y), 0.0f) * w3.x * w12.y;

    result += tex.SampleLevel(linearSampler, float2(texPos0.x, texPos3.y), 0.0f) * w0.x * w3.y;
    result += tex.SampleLevel(linearSampler, float2(texPos12.x, texPos3.y), 0.0f) * w12.x * w3.y;
    result += tex.SampleLevel(linearSampler, float2(texPos3.x, texPos3.y), 0.0f) * w3.x * w3.y;

    return result;
}
