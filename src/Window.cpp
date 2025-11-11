#include "stdafx.h"
#include "Window.h"

static long gWidth, gHeight;

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

HWND CreateMainWindow(HINSTANCE hInstance, int nShowCmd, LPCWSTR name)
{
	SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);
	const auto screenWidth = GetSystemMetrics(SM_CXSCREEN);
	const auto screenHeight = GetSystemMetrics(SM_CYSCREEN);

	gWidth = screenWidth;
	gHeight = screenHeight;

	HWND mainWindow;

	WNDCLASS wc =
	{
		.style = CS_HREDRAW | CS_VREDRAW,
		.lpfnWndProc = WndProc,
		.cbClsExtra = 0,
		.cbWndExtra = 0,
		.hInstance = hInstance,
		.hIcon = LoadIcon(0, IDI_APPLICATION),
		.hCursor = LoadCursor(0, IDC_ARROW),
		.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH),
		.lpszMenuName = 0,
		.lpszClassName = L"WindowClass"
	};

	if (!RegisterClass(&wc))
	{
		MessageBox(0, L"RegisterClass FAILED", 0, 0);
	}

	mainWindow = CreateWindow(L"WindowClass", name, WS_POPUP, 0, 0, screenWidth, screenHeight, 0, 0, hInstance, 0);
	if (mainWindow == 0)
	{
		MessageBox(0, L"CreateWindow FAILED", 0, 0);
	}

	ShowWindow(mainWindow, nShowCmd);
	UpdateWindow(mainWindow);

	return mainWindow;
}


LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
		return true;

	RECT rect = { 0, 0, gWidth, gHeight };

	switch (msg)
	{
	case WM_LBUTTONDOWN:
		ClipCursor(&rect);
		return 0;

	case WM_LBUTTONUP:
		ClipCursor(nullptr);
		return 0;

	case WM_KEYDOWN:
		if (wParam == VK_ESCAPE)
		{
			DestroyWindow(hWnd);
		}
		return 0;

	case WM_KEYUP:
		return 0;

	case WM_MOUSEMOVE:
	{
		gMouseX = static_cast<float>(GET_X_LPARAM(lParam));
		gMouseY = static_cast<float>(GET_Y_LPARAM(lParam));
		return 0;
	}
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hWnd, msg, wParam, lParam);
}