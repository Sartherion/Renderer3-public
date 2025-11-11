#pragma once

static uint2 debugThreadId = 0;

static const uint debugBufferUavId = 0; //the debug buffer has fixed position 0 in descriptor heap

template <typename T>
void StoreInDebugBuffer(uint byteOffset, T value)
{
    RWByteAddressBuffer debugBuffer = ResourceDescriptorHeap[debugBufferUavId];
    debugBuffer.Store(byteOffset, value);
}

void DebugBufferAtomicIncrement(uint byteOffset)
{
    RWByteAddressBuffer debugBuffer = ResourceDescriptorHeap[debugBufferUavId];
    debugBuffer.InterlockedAdd(byteOffset, 1);
}

static const uint debugTextureUavId = 2; //the debug texture has fixed position 2 in descriptor heap

template <typename T>
void StoreInDebugTexture(uint arraySlice, uint2 pos, T value)
{
    RWTexture2DArray<T> debugTexture = ResourceDescriptorHeap[debugBufferUavId];
    debugTexture[uint3(pos.xy, arraySlice)] = value;
}

void DebugTextureAtomicIncrement(uint arraySlice, uint2 pos)
{
    RWTexture2DArray<uint> debugTexture = ResourceDescriptorHeap[debugBufferUavId]; 
    InterlockedAdd(debugTexture[uint3(pos.xy, arraySlice)], 1);
}