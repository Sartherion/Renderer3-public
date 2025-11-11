#include "stdafx.h"
#include "AppUI.h"

namespace App
{
	void UIContext::Update()
	{
		lightSettings.MenuEntry();
	}

	void LightSettings::MenuEntry()
	{
		if (ImGui::CollapsingHeader("Directional Light Settings", ImGuiTreeNodeFlags_None))
		{
			StackContext stackContext;
			LightSettings& lightSettings = *this;
			char* items[directionalLightsMaxCount];
			for (int i = 0; i < directionalLightsMaxCount; i++)
			{
				items[i] = stackContext.Allocate<char>(16);
				sprintf_s(items[i], 16, "Light %d", i);
			}
			ImGui::Combo("Lights", &lightSettings.selectedLight, items, directionalLightsMaxCount);

			auto& directionalLightData = lightSettings.directionalLightData[lightSettings.selectedLight];
			ImGui::Checkbox("Active", &directionalLightData.isActive);
			ImGui::ColorEdit3("Color", (float*)directionalLightData.color, ImGuiColorEditFlags_DisplayHSV | ImGuiColorEditFlags_HDR);
			ImGui::SliderAngle("Polar Angle", &directionalLightData.polarAngle, 0, 90);
			ImGui::SliderAngle("Azimuthal Angle", &directionalLightData.azimuthalAngle, 0, 360);
		}
	}

	uint32_t LightSettings::ReadDirectionalLightData(Light* outActiveDirectionalLights) const
	{
		uint32_t activeDirectionalLightsCount = 0;
		{
			using namespace DirectX;
			for (const auto& light : directionalLightData)
			{
				if (!light.isActive)
				{
					continue;
				}
				const float azimuthal = light.azimuthalAngle;
				const float polar = light.polarAngle;
				outActiveDirectionalLights[activeDirectionalLightsCount].direction = {
					XMScalarCos(azimuthal) * XMScalarSin(polar),
					XMScalarCos(polar),
					XMScalarSin(azimuthal) * XMScalarSin(polar),
				};
				outActiveDirectionalLights[activeDirectionalLightsCount].color = { light.color[0], light.color[1], light.color[2] };
				activeDirectionalLightsCount++;
			}
		}

		return activeDirectionalLightsCount;
	}

	void UIContext::MenuEntry()
	{
		lightSettings.MenuEntry();
	}
}