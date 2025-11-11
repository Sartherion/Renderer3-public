#include "DDGICommon.hlsli"

struct RootConstants
{
    uint ddgiDataOffset;
};
ConstantBuffer<RootConstants> rootConstants : register(b0);

template<typename T>
void CopyBorderPixels(uint index, uint probeIndex, DDGIAtlasData atlasData);

[numthreads(32, 1, 1)]
void main(uint3 threadId : SV_DispatchThreadID)
{
    DDGIData ddgiData = BufferLoad < DDGIData > (rootConstants.ddgiDataOffset);

    CopyBorderPixels<float3>(threadId.x, threadId.z, ddgiData.irradianceAtlasData); 
    CopyBorderPixels<float2>(threadId.x, threadId.z, ddgiData.depthAtlasData);
    CopyBorderPixels<float3>(threadId.x, threadId.z, ddgiData.radianceAtlasData);
}


template<typename T>
void CopyBorderPixels(uint index, uint probeIndex, DDGIAtlasData atlasData) 
{
    RWTexture2D<T> atlas = ResourceDescriptorHeap[atlasData.uavId];
    uint unpaddedTileSize = atlasData.tileSize - 2;

    if (index >= 2 * unpaddedTileSize) 
    {
        return;
    }

    int2 tileOrigin = GetTileOrigin(probeIndex, atlasData);

    // first unpaddedTileSize threads copy top and left edges, the next unpaddedTileSize threads copy bottom and right edges
    const bool isTopLeftBorder = index < unpaddedTileSize; 

    const int edgeIndex = isTopLeftBorder ? index : 2 * unpaddedTileSize - index - 1;
    const int edgeSrc = isTopLeftBorder ? 1 : unpaddedTileSize;
    const int edgeDst = isTopLeftBorder ? 0 : unpaddedTileSize + 1;

    const T row = atlas.Load(tileOrigin + int2(edgeIndex + 1, edgeSrc));
    const T column = atlas.Load(tileOrigin + int2(edgeSrc, edgeIndex + 1));

    // copy edges
    atlas[tileOrigin.xy + int2(unpaddedTileSize - edgeIndex, edgeDst)] = row;
    atlas[tileOrigin.xy + int2(edgeDst, unpaddedTileSize - edgeIndex)] = column;

    // copy corners //@todo: this results in a lot of inactive threads but ok for now
    const bool firstInEdge = index % unpaddedTileSize == 0;
    const bool lastInEdge = (index + 1) % unpaddedTileSize == 0;
    const int cornerDstY = isTopLeftBorder ? unpaddedTileSize + 1 : 0;
    const int cornerDstX = lastInEdge ? edgeDst : cornerDstY;
    if (firstInEdge || lastInEdge)
    {
        atlas[tileOrigin.xy + int2(cornerDstX, cornerDstY)] = row;
    }
}
