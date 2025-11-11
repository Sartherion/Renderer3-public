#pragma once
#define WIN32_LEAN_AND_MEAN
#include "../external/rapidobj/include/rapidobj/rapidobj.hpp"

#include "BlueNoisePregeneratedData.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <bit>
#include <filesystem>
#include <functional>
#include <malloc.h>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>
#include <tuple>


#include "Windows.h"
#include <windowsx.h> //for lparam macro
#include <wrl.h>
#include <comdef.h> //needed for com_error to turn hr to error
#include <d3d12.h>
#include <dxgi1_5.h>
#include "dxcapi.h"
#include "DirectXCollision.h"
#include "DirectXMath.h"
#include "DirectXTex.h"


#define USE_PIX
#include "pix3.h"
using Microsoft::WRL::ComPtr;

#include "../external/imgui/imgui.h"
#include "../external/imgui/backends/imgui_impl_dx12.h"
#include "../external/imgui/backends/imgui_impl_win32.h"
#include "../external/offsetAllocator/offsetAllocator.hpp"

using DescriptorHeapId = uint32_t;
inline const uint32_t DescriptorHeapInvalidId = uint32_t(-1);
using BufferHeapOffset = uint32_t;
inline const uint32_t BufferHeapInvalidOffset = uint32_t(-1);

#undef max
#undef min

#define CheckForErrors(x) \
{ \
	HRESULT hr__= (x);\
	if(FAILED(hr__))\
	{\
		std::wstring errorMessage = _com_error(hr__).ErrorMessage();\
		std::wstring text = L"********************\n Call to "#x" failed in line " + std::to_wstring(__LINE__) + L" of file " __FILE__ L" with message \n" + errorMessage + L"\n ********************\n";\
		 OutputDebugString(text.c_str());\
	}\
}

template <typename T> 
T* Coalesce(T* ptr, T* fallbackPtr)
{
	return ptr != nullptr ? ptr : fallbackPtr;
}

template <typename T> constexpr auto to_underlying(T enumValue)
{
	return static_cast<std::underlying_type_t<T>>(enumValue);
}

//@todo: this will not work if special characters are used
inline std::wstring stringToWstring(const std::string& inputString)
{
	return std::wstring{ inputString.begin(), inputString.end() };
}

// Converts an ANSI string to a std::wstring
inline std::wstring AnsiToWString(const char* ansiString)
{
	wchar_t buffer[512];
	MultiByteToWideChar(CP_ACP, 0, ansiString, -1, buffer, 512);
	return std::wstring(buffer);
}

inline std::string WStringToAnsi(const wchar_t* wideString)
{
	char buffer[512];
	WideCharToMultiByte(CP_ACP, 0, wideString, -1, buffer, 612, NULL, NULL);
	return std::string(buffer);
}

template<typename T> constexpr T Min(T a, T b)
{
    return a < b ? a : b;
}

template<typename T> constexpr T Max(T a, T b)
{
    return a < b ? b : a;
}

constexpr bool IsAlignedTo(uintptr_t address, size_t alignment)
{
    assert((alignment & (alignment - 1)) == 0);
    return (address & alignment - 1) == 0;
}

constexpr bool IsPowerOfTwo(uint32_t number) 
{
    return (number & number - 1) == 0;
}

//@note:from Gregory, Game Engine Architecture
constexpr uintptr_t Align(uintptr_t address, size_t alignment)
{
    const size_t mask = alignment - 1;
    assert((alignment & mask) == 0);
    return (address + mask) & ~mask;
}

inline uint32_t GetMostSignificantBitPosition(uint32_t mask)
{
    
    unsigned long msbPosition;
	_BitScanReverse(&msbPosition, mask);
    
    return static_cast<uint32_t>(msbPosition);
}

constexpr uint32_t DivisionRoundUp(uint32_t nominator, uint32_t denominator)
{
    return (nominator + denominator - 1) / denominator;
    //return (nominator / denominator) + (nominator % denominator != 0);
}

