#pragma once

enum class MouseButton
{
	Left,
	Middle,
	Right
};

namespace Input
{
	void Init(HWND hwnd);

	void Update(float mouseX, float mouseY);
	int GetTwoWayAction(int posKey, int negKey);
	void GetMouseDelta(float& velocityX, float& velocityY);
	bool IsPressed(MouseButton button);
}