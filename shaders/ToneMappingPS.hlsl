#include "Common.hlsli"

#include "ColorHelpers.hlsli"
#include "FullScreenCommon.hlsli"

struct RootConstants
{
    uint inputSrvId;
    uint colorGradingDataBufferOffset;
};
ConstantBuffer<RootConstants> rootConstants : register(b0);

struct ColorGradingData 
{
    float4 colorAdjustments;
    float4 colorFilter;
};

//@note: following https://catlikecoding.com/unity/tutorials/custom-srp/color-grading/
float3 ColorGrade(float3 color, ColorGradingData colorGradingData)
{
    color = min(color, 60.0f);
    //Exposure
    color *= colorGradingData.colorAdjustments.x;
    //Contrast
    float midgray = 0.4135884f;
    color = (color - midgray) * colorGradingData.colorAdjustments.y + midgray; //@todo: conversion to logc missing
    //Filter
    color *= colorGradingData.colorFilter.rgb;
    color = max(color, 0.0);
    //Hue shift
    color = RgbToHsv(color);
    float hue = color.x + colorGradingData.colorAdjustments.z;
    color.x = RotateHue(hue, 0.0, 1.0);
    color = HsvToRgb(color);
    //Saturation
    float luminance = Luminance(color);
    color =  (color - luminance) * colorGradingData.colorAdjustments.w + luminance;
    color = max(color, 0.0);
    
    return color;
}

// approximated version of HP Duiker's film stock curve
float3 ToneMapFilmicALU(float3 color)
{
    color = max(0, color - 0.004f);
    color = (color * (6.2f * color + 0.5f)) / (color * (6.2f * color + 1.7f) + 0.06f);
    return color;
}

float3 ToneMapReinhard(float3 color)
{
    color /= 1.0f + color;
    return color;
}

float3 ToneMapAces(float3 color)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return saturate((color * (a * color + b)) / (color * (c * color + d) + e));
}

float4 main(Interpolants input) : SV_TARGET
{
    Texture2D inputTexture = ResourceDescriptorHeap[rootConstants.inputSrvId];

    ColorGradingData colorGradingData = BufferLoad < ColorGradingData> (rootConstants.colorGradingDataBufferOffset);

    float3 color = inputTexture.SampleLevel(samplerLinearClamp, input.texCoord, 0).rgb;

    color = ColorGrade(color, colorGradingData);
    color = ToneMapAces(color);
    return float4(color, 1.0f);
}
