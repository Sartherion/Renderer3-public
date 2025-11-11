#pragma once

ComPtr<ID3D12Device10> CreateDevice(ComPtr<IDXGIAdapter> adapter = nullptr);
std::vector<ComPtr<IDXGIAdapter>> GetAdapters();
ComPtr<IDXGIFactory4> CreateDXGIFactory();
ComPtr<ID3D12CommandQueue> CreateCommandQueue(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_DIRECT);
std::pair<ComPtr<ID3D12GraphicsCommandList10>, ComPtr<ID3D12CommandAllocator>> CreateCommandList(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_DIRECT);

bool CheckEssentialFeatures(HWND hwnd, ID3D12Device10* device);

std::wstring GetAdapterName(ComPtr<IDXGIAdapter> adapter);
void LogAdapters(const std::vector<ComPtr<IDXGIAdapter>>& adapters, DXGI_FORMAT displayFormat = DXGI_FORMAT_UNKNOWN);
void LogAdapterOutputs(IDXGIAdapter* adapter, DXGI_FORMAT displayFormat);
void LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT displayFormat);