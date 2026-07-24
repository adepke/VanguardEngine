// Copyright (c) 2019-2022 Andrew Depke

#include <Editor/CvarHelpers.h>
#include <Core/ConsoleVariable.h>

#include <imgui.h>

void InvalidCvar(const entt::hashed_string cvar)
{
	ImGui::BeginDisabled();
	ImGui::Text("Console variable '%s' does not exist!", cvar.data());
	ImGui::EndDisabled();
}

void CvarHelpers::Checkbox(const entt::hashed_string cvar, const std::string& name)
{
	auto* cvarPtr = CvarManager::Get().GetVariable<int>(cvar);

	if (!cvarPtr)
	{
		InvalidCvar(cvar);
		return;
	}

	bool value = *cvarPtr;
	ImGui::Checkbox(name.c_str(), &value);
	if (value != *cvarPtr)
	{
		CvarManager::Get().SetVariable<int>(cvar, value);
	}
}

void CvarHelpers::Slider(const entt::hashed_string cvar, const std::string& name, float min, float max)
{
	auto* cvarPtr = CvarManager::Get().GetVariable<float>(cvar);

	if (!cvarPtr)
	{
		InvalidCvar(cvar);
		return;
	}

	float value = *cvarPtr;
	ImGui::SliderFloat(name.c_str(), &value, min, max);
	if (value != *cvarPtr)
	{
		CvarManager::Get().SetVariable<float>(cvar, value);
	}
}

void CvarHelpers::Slider(const entt::hashed_string cvar, const std::string& name, int min, int max)
{
	auto* cvarPtr = CvarManager::Get().GetVariable<int>(cvar);

	if (!cvarPtr)
	{
		InvalidCvar(cvar);
		return;
	}

	int value = *cvarPtr;
	ImGui::SliderInt(name.c_str(), &value, min, max);
	if (value != *cvarPtr)
	{
		CvarManager::Get().SetVariable<int>(cvar, value);
	}
}

void CvarHelpers::Combo(const entt::hashed_string cvar, const std::string& name, const char* const* items, int itemCount)
{
	auto* cvarPtr = CvarManager::Get().GetVariable<int>(cvar);

	if (!cvarPtr)
	{
		InvalidCvar(cvar);
		return;
	}

	int value = *cvarPtr;
	if (ImGui::Combo(name.c_str(), &value, items, itemCount))
	{
		CvarManager::Get().SetVariable<int>(cvar, value);
	}
}