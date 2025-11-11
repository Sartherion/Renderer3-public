#include "Common.hlsli"
#include "FrameConstants.hlsli"

struct RootConstants
{
    uint outputUavId;
    uint sobolBufferOffset;
    uint scramblingTileBufferOffset;
    uint rankingTileBufferOffset;
};
ConstantBuffer<RootConstants> rootConstants : register(b0);

//@note: adapted from AMD FXR 

// This file is part of the FidelityFX SDK.
//
// Copyright (C) 2024 Advanced Micro Devices, Inc.
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.


// Blue Noise Sampler by Eric Heitz. Returns a value in the range [0, 1].
float SampleRandomNumber(uint pixel_i, uint pixel_j, uint sample_index, uint sample_dimension, uint samples_per_pixel, RootConstants rootConstants) {
    // Wrap arguments
    pixel_i = pixel_i & 127u;
    pixel_j = pixel_j & 127u;
    sample_index = (sample_index % samples_per_pixel) & 255u;
    sample_dimension = sample_dimension & 255u;

#ifndef SPP
#    define SPP 256
#endif

#if SPP == 1
    const uint ranked_sample_index = sample_index ^ 0;
#else
    // xor index based on optimized ranking
    const uint ranked_sample_index = sample_index ^ BufferLoad<uint>(rootConstants.rankingTileBufferOffset, sample_dimension + (pixel_i + pixel_j * 128u) * 8u);
#endif

    // Fetch value in sequence
    uint value = BufferLoad<uint>(rootConstants.sobolBufferOffset, sample_dimension + ranked_sample_index * 256u);

    // If the dimension is optimized, xor sequence value based on optimized scrambling
    value = value ^ BufferLoad<uint>(rootConstants.scramblingTileBufferOffset, (sample_dimension % 8u) + (pixel_i + pixel_j * 128u) * 8u);

    // Convert to float and return
    return (value + 0.5f) / 256.0f;
}

[numthreads(8, 8, 1)] 
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    RWTexture2D<float2> blueNoiseTexture = ResourceDescriptorHeap[rootConstants.outputUavId];
    if (all(dispatch_thread_id.xy < 128)) {
        float2 xi = float2(
            SampleRandomNumber(dispatch_thread_id.x, dispatch_thread_id.y, uint(frameConstants.frameTimings.frameId), 0u, 32, rootConstants),
            SampleRandomNumber(dispatch_thread_id.x, dispatch_thread_id.y, uint(frameConstants.frameTimings.frameId), 1u, 32, rootConstants));
        blueNoiseTexture[dispatch_thread_id.xy] = xi.xy;
    }
}
