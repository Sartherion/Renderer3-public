#pragma once

struct RenderFeatures
{
    uint useDirectDirectionalLights: 1;
    uint useDirectPointLights: 1;
    uint sampleSpecularCubeMap:1;
    uint sampleDiffuseDDGI: 1;
    uint sampleSpecularDDGI: 1;
    uint sampleIndirectDiffuseMap: 1;
    uint sampleIndirectSpecularMap: 1;
    uint sampleAoMap: 1;
    uint sampleShadowMap: 1;
    uint forceLastShadowCascade: 1;
    uint isDebugCamera: 1;
			//@note: it does not make sense to toggle alpha testing with a runtime switch, since early z will be disabled if the shader may use clip
};

RenderFeatures RenderFeaturesDefaults()
{
    RenderFeatures result;
    result.useDirectDirectionalLights = true;
    result.useDirectPointLights = true;
    result.sampleSpecularCubeMap = false;
    result.sampleDiffuseDDGI = true;
    result.sampleSpecularDDGI = true;
    result.sampleIndirectDiffuseMap = false;
    result.sampleIndirectSpecularMap = false;
    result.sampleAoMap = false;
    result.sampleShadowMap = true;
    result.forceLastShadowCascade = false;
    result.isDebugCamera = false;
   
    return result;
    
}

uint GetNthBit(uint input, uint N)
{
    return input >> N & 1;
}

RenderFeatures AsRenderFeatures(uint bits)
{
    RenderFeatures result;
    result.useDirectDirectionalLights = GetNthBit(bits, 0);
    result.useDirectPointLights = GetNthBit(bits, 1);
    result.sampleSpecularCubeMap = GetNthBit(bits, 2);
    result.sampleDiffuseDDGI = GetNthBit(bits, 3);
    result.sampleSpecularDDGI = GetNthBit(bits, 4);
    result.sampleIndirectDiffuseMap = GetNthBit(bits, 5);
    result.sampleIndirectSpecularMap = GetNthBit(bits, 6);
    result.sampleAoMap = GetNthBit(bits, 7);
    result.sampleShadowMap = GetNthBit(bits, 8);
    result.forceLastShadowCascade = GetNthBit(bits, 9);
    result.isDebugCamera = GetNthBit(bits, 10);
    
    return result;
}