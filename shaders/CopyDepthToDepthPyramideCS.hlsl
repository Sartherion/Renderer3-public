struct RootConstants
{
    uint inputSrvId;
    uint outputUavId;
};
ConstantBuffer<RootConstants> rootConstants : register(b0);

static Texture2D<float> inputTexture = ResourceDescriptorHeap[rootConstants.inputSrvId];
static RWTexture2D<float2> outputTexture = ResourceDescriptorHeap[rootConstants.outputUavId]; 

[numthreads(8, 8, 1)]
void main( uint3 threadId : SV_DispatchThreadID )
{
    float input = inputTexture.Load(int3(threadId.xy, 0));
    outputTexture[threadId.xy] = float2(input, input);
}
