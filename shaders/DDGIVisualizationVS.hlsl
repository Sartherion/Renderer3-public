#include "DDGIVisualizationCommon.hlsli"
#include "GeometryData.hlsli"

static const float visualizationSphereScale = 0.1;

[RootSignature(universalRS)]
Interpolants main(uint index : SV_VertexID, uint instance : SV_InstanceID )
{
    VertexBufferData vertexBufferData = LoadVertexBufferData(index, rootConstants.vertexBuffersOffset, rootConstants.vertexCount);

    Interpolants output;
    output.positionOS = vertexBufferData.position;
    output.instance = instance;

    DDGIData ddgiData = BufferLoad < DDGIData > (rootConstants.ddgiDataOffset);
    DDGIProbeGridData gridData = ddgiData.probeGridData;
    float3 probeCenter = GetProbePosition(SpatialProbeIndex(instance, gridData.gridDimensions), gridData);
    vertexBufferData.position *= visualizationSphereScale;
    vertexBufferData.position += probeCenter;
    
    CameraConstants cameraConstants = BufferLoad<CameraConstants>(rootConstants.cameraConstantsOffset);

    output.position = mul(float4(vertexBufferData.position, 1.0), cameraConstants.viewProjectionMatrix);

    return output;
}
