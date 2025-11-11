#pragma once

constexpr uint32_t ComputeMaximumMipLevel(uint32_t width, uint32_t height)
{
    return std::bit_width(Max(width, height));
}

inline DirectX::XMVECTOR XM_CALLCONV FindPerpendicularUnitVector(DirectX::FXMVECTOR inputUnitVector)
{
	using namespace DirectX;
	XMVECTOR output = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
	float xySq = XMVectorGetX(XMVector2LengthSq(inputUnitVector)); 
	if (xySq > 0.00001f) 
	{
		float x = XMVectorGetX(inputUnitVector);
		float a2Sq = x * x / xySq;

		auto intermediate = XMVectorSet(1.0f - a2Sq, a2Sq, 0.0f, 0.0f);
		output = XMVectorSqrt(output);
	}

	return output;
}

inline std::pair<DirectX::XMFLOAT3, DirectX::XMFLOAT3> GetBoundingBoxMinMax(const DirectX::BoundingBox& boundingBox)
{
	using namespace DirectX;

	XMVECTOR center = XMLoadFloat3(&boundingBox.Center);
	XMVECTOR extents = XMLoadFloat3(&boundingBox.Extents);

	XMVECTOR min = center - extents;
	XMVECTOR max = center + extents;

    XMFLOAT3 outMin, outMax;
	XMStoreFloat3(&outMin, min);
	XMStoreFloat3(&outMax, max);

    return { outMin, outMax };
}

//@note: the following helpers taken from https://gist.github.com/TheRealMJP/c83b8c0f46b63f3a88a5986f4fa982b1

// Linear interpolation
template<typename T> T Lerp(const T& x, const T& y, float s)
{
    return x + (y - x) * s;
}

// Clamps a value to the specified range
template<typename T> T Clamp(T val, T min, T max)
{
    Assert_(max >= min);

    if (val < min)
        val = min;
    else if (val > max)
        val = max;
    return val;
}

// Clamps a value to [0, 1]
template<typename T> T Saturate(T val)
{
    return Clamp<T>(val, T(0.0f), T(1.0f));
}

// Rounds a float
inline float Round(float r)
{
    return (r > 0.0f) ? std::floorf(r + 0.5f) : std::ceilf(r - 0.5f);
}

// Returns a random float value between 0 and 1
inline float RandFloat()
{
    return rand() / static_cast<float>(RAND_MAX);
}

// Returns x * x
template<typename T> T Square(T x)
{
    return x * x;
}

// Returns the fractional part of x
inline float Frac(float x)
{
    float intPart;
    return std::modf(x, &intPart);
}

// sRGB -> linear conversion
inline float SrgbToLinear(float srgbValue) 
{
    float x = srgbValue / 12.92f;
    float y = std::pow((srgbValue + 0.055f) / 1.055f, 2.4f);

    return srgbValue <= 0.04045f ? x : y;;
}

inline float ComputeLuminance(float r, float g, float b)
{
    return r * 0.299f + g * 0.587f + b * 0.114f;
}

template <typename T> class Average
{
public:
	Average(uint32_t sampleCount) : sampleCount(sampleCount), sampleHistory(sampleCount, 0) {}
	void AddSample(T sample)
	{
		sum += sample;
		sampleHistory[counter] = sample;

		if (++counter >= sampleCount)
		{
			latestAverage = sum / counter;
			sum = T{};
			counter = 0;
		}
	}

	T GetLatestAverage() const
	{
		return latestAverage;
	}

	std::vector<T> sampleHistory;
	uint32_t counter = 0;
private:
	uint32_t sampleCount;

	T sum{};
	T latestAverage{};
};
