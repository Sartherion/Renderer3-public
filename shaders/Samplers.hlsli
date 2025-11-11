#pragma once

SamplerState samplerPointClamp : register(s0);
SamplerState samplerPointWrap : register(s1);
SamplerState samplerLinearClamp : register(s2);
SamplerState samplerLinearWrap : register(s3);
SamplerState samplerTrilinearClamp: register(s4);
SamplerState samplerAnistropicWrap : register(s5);
SamplerState samplerAnistropicClamp : register(s6); 

SamplerState samplerMinimum : register(s7);
SamplerState samplerMaximum : register(s8);

SamplerComparisonState samplerShadowMap : register(s9); //@note: for comparison sampling a special SamplerComparison type needs to be utilized
