#pragma once

float Luminance(float3 color)
{
    return dot(color, float3(0.299f, 0.587f, 0.114f)); 
}

//from https://gist.github.com/983/e170a24ae8eba2cd174f and http://lolengine.net/blog/2013/07/27/rgb-to-hsv-in-glsl
float3 HsvToRgb(float3 colorHsv)
{
    float4 K = float4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    float3 p = abs(frac(colorHsv.xxx + K.xyz) * 6.0 - K.www);

    return colorHsv.z * lerp(K.xxx, saturate(p - K.xxx), colorHsv.y);
}

float3 RgbToHsv(float3 colorRgb)
{
    float4 K = float4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    float4 p = colorRgb.g < colorRgb.b ? float4(colorRgb.bg, K.wz) : float4(colorRgb.gb, K.xy);
    float4 q = colorRgb.r < p.x ? float4(p.xyw, colorRgb.r) : float4(colorRgb.r, p.yzx);

    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return float3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

//from https://github.com/Unity-Technologies/FPSSample/blob/master/Packages/com.unity.postprocessing/PostProcessing/Shaders/Colors.hlsl
float RotateHue(float value, float low, float hi)
{
    return (value < low)
            ? value + hi
            : (value > hi)
                ? value - hi
                : value;
}
