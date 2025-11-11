#pragma once
#include "Renderer.h"

namespace App
{
	constexpr uint32_t directionalLightsMaxCount = 4;

	struct LightSettings
	{
		int selectedLight = 0;

		struct DirectionalLightData
		{
			float polarAngle = 0.0f;
			float azimuthalAngle = 0.0f;
			float color[3] = { 1.0f, 1.0f, 1.0f };
			bool isActive = false;
		};
		DirectionalLightData directionalLightData[directionalLightsMaxCount];

		void MenuEntry();
		uint32_t ReadDirectionalLightData(Light* outActiveDirectionalLights) const;
	};

	struct UIContext : UI::AppMenuBase
	{
		LightSettings lightSettings;
		void Update();
		virtual void MenuEntry() override;
	};
}
