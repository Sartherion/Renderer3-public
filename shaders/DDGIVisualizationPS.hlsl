#include "DDGIVisualizationCommon.hlsli"

float4 main(Interpolants input) : SV_TARGET
{
    DDGIData ddgiData = BufferLoad<DDGIData>(rootConstants.ddgiDataOffset);

    uint atlasSrvId = ddgiData.irradianceAtlasData.srvId;
    DDGIAtlasData atlasData = ddgiData.irradianceAtlasData;
    if (frameConstants.settings.debugVisualizationSettings.isRadianceDDGIVisualization)
    {
        atlasSrvId = ddgiData.radianceAtlasData.srvId;
        atlasData = ddgiData.radianceAtlasData;
    }
    Texture2D<float3> atlas = ResourceDescriptorHeap[atlasSrvId];

    float2 uv = OctahedralEncode(input.positionOS);

    const uint tileSize = atlasData.tileSize;
    uv = TileUvToPaddedTileUv(uv, tileSize);
    uv = PaddedTileUvToAtlasUv(uv, input.instance, atlasData);

    return float4(atlas.SampleLevel(samplerLinearWrap, uv, 0).rgb, input.instance);
}
