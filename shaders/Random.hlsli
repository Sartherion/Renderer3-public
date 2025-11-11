#pragma once
#include "MathHelpers.hlsli"

// random number generation

//@note: taken from https://www.reedbeta.com/blog/hash-functions-for-gpu-rendering/
static uint rng_state;

void SeedRng(uint seed)
{
    rng_state = seed;
}
uint pcg_hash(uint input)
{
    uint state = input * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

uint rand_pcg()
{
    uint state = rng_state;
    rng_state = rng_state * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

float RandomFloat()
{
    return rand_pcg() * 1.0f / 4294967296.0f;
}

//@note: according to Raytracing Gems II ch. 14
// 32-bit Xorshift random number generator
uint xorshift(inout uint rngState)
{
	rngState ^= rngState << 13;
	rngState ^= rngState >> 17;
	rngState ^= rngState << 5;
	return rngState;
}

// Jenkins's "one at a time" hash function
uint jenkinsHash(uint x) {
	x += x << 10;
	x ^= x >> 6;
	x += x << 3;
	x ^= x >> 11;

	x += x << 15;
	return x;
}

// Converts unsigned integer into float int range <0; 1) by using 23 most significant bits for mantissa
float uintToFloat(uint x) {
	return asfloat(0x3f800000 | (x >> 9)) - 1.0f;
}

uint initRNG(uint2 pixelCoords, uint2 resolution, uint frameNumber) {
	uint seed = dot(pixelCoords, uint2(1, resolution.x)) ^ jenkinsHash(frameNumber);
	return jenkinsHash(seed);
}

// Return random float in <0; 1) range (Xorshift-based version)
float rand(inout uint rngState) {
	return uintToFloat(xorshift(rngState));
}

//@note: c.f. https://blog.demofox.org/2022/01/01/interleaved-gradient-noise-a-different-kind-of-low-discrepancy-sequence/
float InterleavedGradientNoise(int2 pixel)
{
    return fmod(52.9829189f * fmod(0.06711056f * pixel.x + 0.00583715f * pixel.y, 1.0f), 1.0f);
}

// importance sampling

//@note: trimming factor introduced by https://gdcvault.com/play/1026701/Fast-Denoising-With-Self-Stabilizing presentation
// http://jcgt.org/published/0007/04/01/paper.pdf by Eric Heitz
// Input Ve: view direction
// Input alpha_x, alpha_y: roughness parameters
// Input U1, U2: uniform random numbers
// Output Ne: normal sampled with PDF D_Ve(Ne) = G1(Ve) * max(0, dot(Ve, Ne)) * D(Ne) / Ve.z
float3 SampleGGXVNDF(float3 Ve, float alpha_x, float alpha_y, float U1, float U2)
{
    #ifndef USE_GGX 
    // Section 3.2: transforming the view direction to the hemisphere configuration
    float3 Vh = normalize(float3(alpha_x * Ve.x, alpha_y * Ve.y, Ve.z));
    // Section 4.1: orthonormal basis (with special case if cross product is zero)
    float lensq = Vh.x * Vh.x + Vh.y * Vh.y;
    float3 T1 = lensq > 0 ? float3(-Vh.y, Vh.x, 0) * rsqrt(lensq) : float3(1, 0, 0);
    float3 T2 = cross(Vh, T1);
    // Section 4.2: parameterization of the projected area
    float r = sqrt(U1);
    float phi = 2.0 * PI * U2;
    float t1 = r * cos(phi);
    float t2 = r * sin(phi);
    float s = 0.5 * (1.0 + Vh.z);
    t2 = (1.0 - s) * sqrt(1.0 - t1 * t1) + s * t2;
    // Section 4.3: reprojection onto hemisphere
    float3 Nh = t1 * T1 + t2 * T2 + sqrt(max(0.0, 1.0 - t1 * t1 - t2 * t2)) * Vh;
    // Section 3.4: transforming the normal back to the ellipsoid configuration
    float3 Ne = normalize(float3(alpha_x * Nh.x, alpha_y * Nh.y, max(0.0, Nh.z)));
    return Ne;
    #else //standard ggx sampling 
    float cosTheta2 = (1.0 - U1) / ((alpha_x * alpha_x - 1) * U1 + 1);
    float cosTheta = sqrt(cosTheta2);
    float sinTheta = sqrt(1 - cosTheta2);
    float phi = 2.0 * PI * U2;
    float cosPhi = cos(phi);
    float sinPhi = sin(phi);
    
    return float3(sinTheta * cosPhi, sinTheta * sinPhi, cosTheta); //@todo: might have wrong handedness
    #endif
}

//@note: adapted from AMD FFX framework
float3 SampleGGXVNDFReflectionVector(float3 viewDirection, float3 normal, float roughness, float2 randomVector)
{
    float3x3 TBNMatrix = (float3x3) Rotation(normal);

    float3 viewDirectionTBN = mul(-viewDirection, transpose(TBNMatrix));

    float3 sampledNormalTBN = SampleGGXVNDF(viewDirectionTBN, roughness, roughness, randomVector.x, randomVector.y);

    float3 reflectedDirectionTBN = reflect(-viewDirectionTBN, sampledNormalTBN);

    return mul(reflectedDirectionTBN, TBNMatrix);
}

//@note: adapted from Raytracing Gems I ch. 16
float3 SampleCosineWeightedHemisphere(float2 u, out float pdf)
{

    float a = sqrt(u.x);
    float b = 2 * PI * u.y;

    float3 result = float3(
		a * cos(b),
		a * sin(b),
		sqrt(1.0f - u.x));

    pdf = result.z * 1.0 / PI;

    return result;
}

float3 SampleFullSphere(float2 u) 
{
    float phi = 2 * PI * u.y;
    float cosPhi = cos(phi);
    float sinPhi = sin(phi);
    float cosTheta = 1.0 - 2.0 * u.x;
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    return float3(sinTheta * cosPhi, sinTheta * sinPhi, cosTheta);
}

float3 FibonacciSpiral(uint i, uint sampleCount)
{
    const float goldenRatio = 0.5 + 0.5 * sqrt(2);

    float2 u = float2((i + 0.5) / sampleCount, frac((goldenRatio - 1.0) * i));
    return SampleFullSphere(u);
}
