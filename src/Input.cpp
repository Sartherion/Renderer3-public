#include "stdafx.h"
#include "Input.h"

namespace Input
{
	static HWND window;
	static float mMouseX;
	static float mMouseY;
	static float mPreviousMouseX;
	static float mPreviousMouseY;

	void Init(HWND hwnd)
	{
		window = hwnd;
	}

	void Update(float mouseX, float mouseY)
	{
		mPreviousMouseX = mMouseX;
		mPreviousMouseY = mMouseY;

		mMouseX = mouseX;
		mMouseY = mouseY;
	}

	int GetTwoWayAction(int posKey, int negKey)
	{
		if (window && GetForegroundWindow() != window)
		{
			return 0;
		}

		int returnValue = GetAsyncKeyState(posKey) ? 1 : 0;
		returnValue -= GetAsyncKeyState(negKey) ? 1 : 0;

		return returnValue;
	}

	void GetMouseDelta(float& velocityX, float& velocityY)
	{
		velocityX = (mMouseX - mPreviousMouseX);
		velocityY = (mMouseY - mPreviousMouseY);
	}

	bool IsPressed(MouseButton button)
	{
		switch (button)
		{
		case MouseButton::Left:
			return GetAsyncKeyState(VK_LBUTTON);
		case MouseButton::Middle:
			return GetAsyncKeyState(VK_MBUTTON);
		case MouseButton::Right:
			return GetAsyncKeyState(VK_RBUTTON);
		}
		return false;
	}
}
