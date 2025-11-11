#include "Common.hlsli"
#include "ShaderFastMath.hlsli"
#include "DDGICommon.hlsli"
#include "Random.hlsli"
#include "ViewHelpers.hlsli"

//#define COMPUTE_SSGI
//#define SAMPLE_AMBIENT

const static uint sectorCount = 32;

struct RootConstants
{
    uint outputTextureUavId;
    uint historyLitBufferSrvId;
    uint ddgiDataOffset;
};
ConstantBuffer<RootConstants> rootConstants : register(b0);

uint UpdateSectors(float minHorizon, float maxHorizon, uint globalOccludedBitfield);
float3 SampleAmbient(uint bitfield, float3 position, float3 viewVector, float3 projectedNormal, float3 projectedNormalHemisphereTangent, DDGIData ddgiData); 

//@note: implementation based on https://cdrinmatane.github.io/posts/ssaovb-code/
[numthreads(8, 8, 1)] 
void main( uint3 threadId: SV_DispatchThreadID)
{
    const uint2 textureSize = frameConstants.mainRenderTargetDimensions.size;
    const float2 texelSize = frameConstants.mainRenderTargetDimensions.texelSize;
    CameraConstants cameraConstants = frameConstants.mainCameraData;

    RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[rootConstants.outputTextureUavId]; 
    Texture2D<float4> historyLitBuffer = ResourceDescriptorHeap[rootConstants.historyLitBufferSrvId];
    DDGIData ddgiData = BufferLoad < DDGIData > (rootConstants.ddgiDataOffset);
    GBuffers gBuffers = LoadGBuffers(frameConstants.gBufferSrvIds);

    const float2 screenUv = (threadId.xy + 0.5) * texelSize;
    const float nonlinearDepth = gBuffers.depth.Load(int3(threadId.xy, 0));
    const float linearDepth = NonlinearToLinearDepth(nonlinearDepth, cameraConstants.projectionMatrix);
    const float3 positionWS = SSToWS(screenUv, nonlinearDepth, cameraConstants.inverseViewProjectionMatrix);
    const float3 positionVS = mul(float4(positionWS, 1), cameraConstants.viewMatrix).xyz;
    const float3 viewVectorWS = normalize(cameraConstants.position.xyz - positionWS);
    const float3 viewVectorVS = -normalize(positionVS);
    const float3 normalWS = SampleNormalWS(gBuffers, screenUv);
    const float3 normalVS = mul(float4(normalWS, 0), cameraConstants.viewMatrix).xyz;

    const float2 radiusSS = ProjectSphereRadius(frameConstants.settings.ssaoSettings.radius, linearDepth, cameraConstants.projectionMatrix) * textureSize;
    const float2 stepSize = float2(1.0, -1.0) * radiusSS / (frameConstants.settings.ssaoSettings.stepsCount + 1);

    uint seed = initRNG(threadId.xy, textureSize, uint(frameConstants.frameTimings.frameId));
    SeedRng(uint(frameConstants.frameTimings.frameId));
    const float phiOffset = InterleavedGradientNoise(threadId.xy + rand_pcg() % 1000) * 2 * PI;

    const float2 rayStart = threadId.xy + 0.5;
    const float deltaPhi = PI / frameConstants.settings.ssaoSettings.azimuthalDirectionsCount;

    float occlusion = 0;
    float3 indirectLighting = 0;
    for (uint i = 0; i < frameConstants.settings.ssaoSettings.azimuthalDirectionsCount; i++)
    {
        const float phi = i * deltaPhi + phiOffset;
        const float3 directionVS = float3(cos(phi), sin(phi), 0);

        const float3 sliceN = normalize(cross(directionVS, viewVectorVS));
        const float3 projN = normalVS - sliceN * dot(normalVS, sliceN);
        const float lengthProjN = length(projN);
        const float cosN = dot(projN / lengthProjN, viewVectorVS);
    
        const float3 T = cross(viewVectorVS, sliceN);
        const float N = -sign(dot(projN, T)) * acosFast4(cosN);
    
        const float offset = frameConstants.settings.ssaoSettings.useOffsetJitter ? rand(seed) - 0.5 : 0.0;
        const int stepsCount = frameConstants.settings.ssaoSettings.stepsCount;

        uint occlusionBitfield = 0;
        for (int i = -stepsCount; i < stepsCount; i++)
        {
            const int samplingDirection = i < 0 ? -1 : 1;
            const float2 t = (offset + 0.5 + i) * stepSize;
            const float2 samplePosition = rayStart + t * directionVS.xy;
            const float2 sampleUv = samplePosition / textureSize;
            const float sampleDepth = gBuffers.depth.SampleLevel(samplerPointClamp, sampleUv, 0);
            const float3 sampleNormalVS = mul(float4(SampleNormalWS(gBuffers, sampleUv), 0.0), cameraConstants.viewMatrix).xyz;
            const float3 samplePositionVS = SSToWS(sampleUv, sampleDepth, cameraConstants.inverseProjectionMatrix);

            const float3 deltaPosition = samplePositionVS - positionVS;
            const float3 deltaPositionBackface = deltaPosition - viewVectorVS * frameConstants.settings.ssaoSettings.thickness;

            float2 frontBackHorizon = float2(dot(normalize(deltaPosition), viewVectorVS), dot(normalize(deltaPositionBackface), viewVectorVS));
            frontBackHorizon.x = acosFast4(frontBackHorizon.x);
            frontBackHorizon.y = acosFast4(frontBackHorizon.y);
            frontBackHorizon = saturate(((samplingDirection * (-frontBackHorizon)) - N + PI * 0.5) / PI);
            frontBackHorizon = samplingDirection >= 0 ? frontBackHorizon.yx : frontBackHorizon.xy;
        
            const uint indirectLightingBitfield = UpdateSectors(frontBackHorizon.x, frontBackHorizon.y, 0);

            #ifdef COMPUTE_SSGI 
            const float3 normalizedDeltaPosition = normalize(deltaPosition);
            float3 lightSample = historyLitBuffer.SampleLevel(samplerLinearClamp, sampleUv, 0).rgb;
            indirectLighting += (1.0 - float(countbits(indirectLightingBitfield & ~occlusionBitfield)) / sectorCount) * lightSample
            * saturate(max(0.1, dot(normalizedDeltaPosition, normalVS)))
            * saturate(max(0.1, dot(sampleNormalVS, -normalizedDeltaPosition)));
            #endif
      
            occlusionBitfield |= indirectLightingBitfield;
        }
    
        occlusion += 1.0 - float(countbits(occlusionBitfield)) / sectorCount;
        
        #ifdef SAMPLE_AMBIENT
        indirectLighting += SampleAmbient(occlusionBitfield, positionWS, viewVectorWS, normalize(projN), normalize(cross(projN, sliceN)), ddgiData);
        #endif

    }
    outputTexture[threadId.xy] = float4(indirectLighting, occlusion) / frameConstants.settings.ssaoSettings.azimuthalDirectionsCount;
}

uint UpdateSectors(float minHorizon, float maxHorizon, uint globalOccludedBitfield)
{
    uint startHorizonInt = minHorizon * sectorCount;
    uint angleHorizonInt = ceil((maxHorizon - minHorizon) * sectorCount);
    uint angleHorizonBitfield = angleHorizonInt > 0 ? (0xFFFFFFFFu >> (sectorCount - angleHorizonInt)) : 0;
    uint currentOccludedBitfield = angleHorizonBitfield << startHorizonInt;
    return globalOccludedBitfield | currentOccludedBitfield;
}

float3 SampleAmbient(uint bitfield, float3 position, float3 viewVector, float3 projectedNormal, float3 projectedNormalHemisphereTangent, DDGIData ddgiData) 
{
    const uint sampleCount = 4;
    const float deltaTheta = PI / sampleCount;
    const uint sectorSize = sectorCount / sampleCount; 
    const uint baseMask = 0xFF;

    float3 ambient = 0;
    for (uint i = 0; i < sampleCount; i++)
    {
        //theta angle w.r.t. tangent axis
        const float theta = PI - (i + 0.5) * deltaTheta;
        const float3 sampleVectorVS = sin(theta) * projectedNormal + cos(theta) * projectedNormalHemisphereTangent;
        const float3 sampleVectorWS = mul(float4(sampleVectorVS, 0), frameConstants.mainCameraData.inverseViewMatrix).xyz;

        const uint mask = baseMask << i * sectorSize;
        const float weight = float( sectorSize - countbits(bitfield & mask)) / sectorSize;
        ambient += weight * SampleDDGIGrid(position, viewVector, sampleVectorWS, ddgiData);
    }
    return ambient; //no division by sum of weights, since this is just ambient occlusion, so no need to multiply with ao later
}
