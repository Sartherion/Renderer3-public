#include "stdafx.h"
#include "D3DInitHelpers.h"

extern "C" 
{
	__declspec(dllexport) extern const UINT D3D12SDKVersion = D3D12_PREVIEW_SDK_VERSION;//  D3D12_SDK_VERSION;
	__declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\";
}

ComPtr<IDXGIFactory4> CreateDXGIFactory()
{
	ComPtr<IDXGIFactory4> dxgiFactory;
	UINT createFactoryFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)
	createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif
	CheckForErrors(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory)));
	return dxgiFactory;
}

ComPtr<ID3D12Device10> CreateDevice(ComPtr<IDXGIAdapter> adapter)
{
	ComPtr<ID3D12Device10> device;

	//Enable debug layer for debug builds
#if defined(_DEBUG)
	ComPtr<ID3D12Debug> debugController;
	CheckForErrors(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
	debugController->EnableDebugLayer();
#endif

#if defined(EXPERIMENTAL)
	UUID Features[2] =
	{ D3D12ExperimentalShaderModels, D3D12StateObjectsExperiment };
	HRESULT hr = D3D12EnableExperimentalFeatures(_countof(Features), Features,
		nullptr, nullptr);
#else
	UUID Features[] =
	{ D3D12ExperimentalShaderModels };
	HRESULT hr = D3D12EnableExperimentalFeatures(_countof(Features), Features,
		nullptr, nullptr);
#endif
	
	//Create device from given adapter
	CheckForErrors(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_2, IID_PPV_ARGS(&device)));

	//enable break on debug layer error
#ifdef _DEBUG
	ComPtr<ID3D12InfoQueue> infoQueue;
	device->QueryInterface(IID_PPV_ARGS(&infoQueue));
	infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
#endif

	return device;
}

ComPtr<ID3D12CommandQueue> CreateCommandQueue(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE type) 
{
	ComPtr<ID3D12CommandQueue> commandQueue;
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = type;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	CheckForErrors(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue)));
	return commandQueue;
}

std::vector<ComPtr<IDXGIAdapter>> GetAdapters()
{
	auto dxgiFactory = CreateDXGIFactory();
	UINT i = 0;
	ComPtr<IDXGIAdapter> adapter;
	std::vector<ComPtr<IDXGIAdapter>> adapterList;
	while (dxgiFactory->EnumAdapters(i, &adapter) != DXGI_ERROR_NOT_FOUND)
	{
		adapterList.push_back(adapter);
		++i;
	}
	return adapterList;
}

std::wstring GetAdapterName(ComPtr<IDXGIAdapter> adapter)
{
	DXGI_ADAPTER_DESC desc;
	adapter->GetDesc(&desc);
	return desc.Description;
}

void LogAdapters(const std::vector<ComPtr<IDXGIAdapter>>& adapters, DXGI_FORMAT displayFormat)
{
	for(const auto& adapter : adapters)
	{
		std::wstring text = L"***Adapter: ";
		text += GetAdapterName(adapter);
		text += L"\n";
		OutputDebugString(text.c_str());
		LogAdapterOutputs(adapter.Get(), displayFormat);
	}
}

void LogAdapterOutputs(IDXGIAdapter* adapter, DXGI_FORMAT displayFormat)
{
	UINT i = 0;
	IDXGIOutput* output = nullptr;
	while (adapter->EnumOutputs(i, &output) != DXGI_ERROR_NOT_FOUND)
	{
		DXGI_OUTPUT_DESC desc;
		output->GetDesc(&desc);
		std::wstring text = L"***Output: ";
		text += desc.DeviceName;
		text += L"\n";
		OutputDebugString(text.c_str());

		if (displayFormat != DXGI_FORMAT_UNKNOWN)
		{ 
			LogOutputDisplayModes(output, displayFormat);
		}
		output->Release();
		i++;
	}
}

void LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT displayFormat)
{
	UINT count = 0;
	UINT flags = 0;

	output->GetDisplayModeList(displayFormat, flags, &count, nullptr);

	std::vector<DXGI_MODE_DESC> modeList(count);

	output->GetDisplayModeList(displayFormat, flags, &count, modeList.data());

	for (auto& mode : modeList)
	{
		UINT n = mode.RefreshRate.Numerator;
		UINT d = mode.RefreshRate.Denominator;
		std::wstring text = L"Width = " + std::to_wstring(mode.Width) + L" " +
			L"Height = " + std::to_wstring(mode.Height) + L" " +
			L"Refresh = " + std::to_wstring(n) + L"/" + std::to_wstring(d) + L"\n";

		OutputDebugString(text.c_str());
	}
}

bool CheckEssentialFeatures(HWND hwnd, ID3D12Device10* device)
{
	{
		D3D12_FEATURE_DATA_D3D12_OPTIONS12 feature;
		device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS12, &feature, sizeof(feature));
		if (!feature.EnhancedBarriersSupported)
		{
			MessageBox(hwnd, L"Enhanced Barrier Support Required", nullptr, MB_OK);
			return false;
		}
	}

	{
		D3D12_FEATURE_DATA_D3D12_OPTIONS16 feature;
		device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS16, &feature, sizeof(feature));
		if (!feature.GPUUploadHeapSupported)
		{
			MessageBox(hwnd, L"GPU Upload Heap Support Required", nullptr, MB_OK);
			return false;
		}
	}

	return true;
}