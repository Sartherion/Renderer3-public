#pragma once

struct ShadingProducts 
{
    float NdotV;
    float NdotL;
    float LdotH;
    float NdotH;
};

//@note: modified
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248926
// http://go.microsoft.com/fwlink/?LinkId=248929
// http://go.microsoft.com/fwlink/?LinkID=615561

static const float EPSILON = 1e-6f;

// Specular coefficiant - fixed reflectance value for non-metals
static const float kSpecularCoefficient = 0.04;

// Shlick's approximation of Fresnel
// https://en.wikipedia.org/wiki/Schlick%27s_approximation
float3 Fresnel_Shlick(in float3 f0, in float3 f90, in float x)
{
    return f0 + (f90 - f0) * pow(1.f - x, 5.f);
}

// Burley B. "Physically Based Shading at Disney"
// SIGGRAPH 2012 Course: Practical Physically Based Shading in Film and Game Production, 2012.
float Diffuse_Burley(ShadingProducts products, float roughness)
{
    float fd90 = 0.5f + 2.f * roughness * products.LdotH * products.LdotH;
    return Fresnel_Shlick(1, fd90, products.NdotL).x * Fresnel_Shlick(1, fd90, products.NdotV).x;
}

// GGX specular D (normal distribution)
// https://www.cs.cornell.edu/~srm/publications/EGSR07-btdf.pdf
float Specular_D_GGX(in float alpha, in float NdotH)
{
    const float alpha2 = alpha * alpha;
    const float lower = (NdotH * NdotH * (alpha2 - 1)) + 1;
    return alpha2 / max(EPSILON, PI * lower * lower);
}

// Schlick-Smith specular G (visibility) with Hable's LdotH optimization
// http://www.cs.virginia.edu/~jdl/bib/appearance/analytic%20models/schlick94b.pdf
// http://graphicrants.blogspot.se/2013/08/specular-brdf-reference.html
float G_Shlick_Smith_Hable(float alpha, float LdotH)
{
    return rcp(lerp(LdotH * LdotH, 1, alpha * alpha * 0.25f));
}

// A microfacet based BRDF.
//
// alpha:           This is roughness * roughness as in the "Disney" PBR model by Burley et al.
//
// specularColor:   The F0 reflectance value - 0.04 for non-metals, or RGB for metals. This follows model 
//                  used by Unreal Engine 4.
//
// NdotV, NdotL, LdotH, NdotH: vector relationships between,
//      N - surface normal
//      V - eye normal
//      L - light normal
//      H - half vector between L & V.
float3 Specular_BRDF(in float alpha, in float3 specularColor, ShadingProducts products)
{
    // Specular D (microfacet normal distribution) component
    float specular_D = Specular_D_GGX(alpha, products.NdotH);

    // Specular Fresnel
    float3 specular_F = Fresnel_Shlick(specularColor, 1, products.LdotH);

    // Specular G (visibility) component
    float specular_G = G_Shlick_Smith_Hable(alpha, products.LdotH);

    return specular_D * specular_F * specular_G;
}

// Diffuse irradiance
float3 Diffuse_IBL(in float3 N, TextureCube IrradianceTexture)
{
    return IrradianceTexture.Sample(samplerLinearClamp, N).rgb;
}

// Approximate specular image based lighting by sampling radiance map at lower mips 
// according to roughness, then modulating by Fresnel term. 
float3 Specular_IBL(float3 direction, float lodBias, uint arrayIndex, TextureCubeArray RadianceTexture)
{
    int NumRadianceMipLevels = 12;
    float mip = lodBias * (NumRadianceMipLevels + 5);
    return clamp(RadianceTexture.SampleLevel(samplerLinearClamp, float4(direction, arrayIndex), mip).rgb, 0, FLT_MAX);
}
