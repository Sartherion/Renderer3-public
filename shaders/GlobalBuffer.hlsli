#pragma once

ByteAddressBuffer globalBuffer : register(t0);

//Wrapper around templateted loads / stores with offset in given in number of elements of type T instead of bytes.
template <typename T>
void BufferStore(RWByteAddressBuffer buffer, T value, uint elementOffset)
{
    buffer.Store(elementOffset * sizeof(T), value);
}

template <typename T>
T BufferLoad(ByteAddressBuffer buffer, uint elementOffset)
{
    return buffer.Load < T > (elementOffset * sizeof(T));
}

template <typename T>
T BufferLoad(RWByteAddressBuffer buffer, uint elementOffset)
{
    return buffer.Load < T > (elementOffset * sizeof(T));
}

struct RWBufferWithCounter
{
    RWByteAddressBuffer buffer;
    uint IncrementCounter(uint amount = 1)
    {
        uint indexCount;
        buffer.InterlockedAdd(0, amount, indexCount); //counter at 0
        return indexCount;
    }

    template<typename T> T Load(uint elementOffset)
    {
        return buffer.Load < T > (16 + elementOffset * sizeof(T)); //data begins at 16B
    }
    
    template<typename T> void Store(uint elementOffset, T value)
    {
        buffer.Store(16 + elementOffset * sizeof(T), value); //data begins at 16B
    }
};

template<typename T>
T BufferLoad(uint bufferOffset, uint elementIndex = 0)
{
    return globalBuffer.Load< T > (bufferOffset + sizeof(T) * elementIndex);

}
