// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <Rendering/Device.h>
#include <Rendering/ResourceHandle.h>
#include <Rendering/ResourceManager.h>

#include <imgui.h>

namespace ImGui
{
	// Note: we need to use static descriptors instead of dynamic descriptors, since ImGui uses the texture ID as component of the item id, so it cannot change between frames.
	// We can have an Image() overload for a custom descriptor, but not an ImageButton() overload without some changes to the ID system.

	inline void Image(RenderDevice* device, TextureHandle handle, const ImVec2& scale = { 1.f, 1.f }, const ImVec2& uv0 = { 0.f, 0.f }, const ImVec2& uv1 = { 1.f, 1.f }, const ImVec4& tint = { 1.f, 1.f, 1.f, 1.f })
	{
		if (!device->GetResourceManager().Valid(handle)) return;

		const auto& textureComponent = device->GetResourceManager().Get(handle);
		ImGui::Image((ImTextureID)textureComponent.SRV->bindlessIndex, { (float)textureComponent.description.width * scale.x, (float)textureComponent.description.height * scale.y }, uv0, uv1, tint);
	}

	inline void ImageButton(RenderDevice* device, TextureHandle handle, const ImVec2& scale = { 1.f, 1.f }, const ImVec2& uv0 = { 0.f, 0.f }, const ImVec2& uv1 = { 1.f, 1.f }, const ImVec4& tint = { 1.f, 1.f, 1.f, 1.f })
	{
		if (!device->GetResourceManager().Valid(handle)) return;

		const auto& textureComponent = device->GetResourceManager().Get(handle);
		ImGui::ImageButton("", (ImTextureID)textureComponent.SRV->bindlessIndex, { (float)textureComponent.description.width * scale.x, (float)textureComponent.description.height * scale.y }, uv0, uv1, tint);
	}

	inline void StyleColorsVanguard(ImGuiStyle* dst = nullptr)
	{
		ImGuiStyle* style = dst ? dst : &ImGui::GetStyle();
		ImVec4* colors = style->Colors;

		colors[ImGuiCol_Text] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
		colors[ImGuiCol_TextDisabled] = ImVec4(0.47f, 0.47f, 0.47f, 1.00f);
		colors[ImGuiCol_WindowBg] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
		colors[ImGuiCol_ChildBg] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
		colors[ImGuiCol_PopupBg] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
		colors[ImGuiCol_Border] = ImVec4(0.08f, 0.08f, 0.08f, 0.71f);
		colors[ImGuiCol_BorderShadow] = ImVec4(1.00f, 1.00f, 1.00f, 0.03f);
		colors[ImGuiCol_FrameBg] = ImVec4(0.09f, 0.09f, 0.09f, 1.00f);
		colors[ImGuiCol_FrameBgHovered] = ImVec4(0.31f, 0.31f, 0.31f, 0.40f);
		colors[ImGuiCol_FrameBgActive] = ImVec4(0.39f, 0.39f, 0.39f, 0.67f);
		colors[ImGuiCol_TitleBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
		colors[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
		colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.17f, 0.17f, 0.17f, 0.90f);
		colors[ImGuiCol_MenuBarBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
		colors[ImGuiCol_ScrollbarBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
		colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.52f, 0.52f, 0.52f, 1.00f);
		colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.76f, 0.76f, 0.76f, 1.00f);
		colors[ImGuiCol_CheckMark] = ImVec4(0.65f, 0.65f, 0.65f, 1.00f);
		colors[ImGuiCol_SliderGrab] = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
		colors[ImGuiCol_SliderGrabActive] = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
		colors[ImGuiCol_Button] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
		colors[ImGuiCol_ButtonHovered] = ImVec4(0.50f, 0.50f, 0.50f, 0.59f);
		colors[ImGuiCol_ButtonActive] = ImVec4(0.65f, 0.65f, 0.65f, 1.00f);
		colors[ImGuiCol_Header] = ImVec4(0.38f, 0.38f, 0.38f, 1.00f);
		colors[ImGuiCol_HeaderHovered] = ImVec4(0.47f, 0.47f, 0.47f, 1.00f);
		colors[ImGuiCol_HeaderActive] = ImVec4(0.76f, 0.76f, 0.76f, 0.77f);
		colors[ImGuiCol_Separator] = ImVec4(0.00f, 0.00f, 0.00f, 0.14f);
		colors[ImGuiCol_SeparatorHovered] = ImVec4(0.71f, 0.71f, 0.71f, 0.27f);
		colors[ImGuiCol_SeparatorActive] = ImVec4(0.71f, 0.71f, 0.71f, 0.78f);
		colors[ImGuiCol_ResizeGrip] = ImVec4(1.00f, 1.00f, 1.00f, 0.24f);
		colors[ImGuiCol_ResizeGripHovered] = ImVec4(1.00f, 1.00f, 1.00f, 0.51f);
		colors[ImGuiCol_ResizeGripActive] = ImVec4(1.00f, 1.00f, 1.00f, 0.81f);
		colors[ImGuiCol_Tab] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
		colors[ImGuiCol_TabHovered] = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
		colors[ImGuiCol_TabActive] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
		colors[ImGuiCol_TabUnfocused] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
		colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
		colors[ImGuiCol_DockingPreview] = ImVec4(0.94f, 0.94f, 0.94f, 0.43f);
		colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
		colors[ImGuiCol_PlotLines] = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
		colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
		colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
		colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
		colors[ImGuiCol_TextSelectedBg] = ImVec4(0.73f, 0.73f, 0.73f, 0.35f);
		colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
		colors[ImGuiCol_NavHighlight] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
		colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
		colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
		colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);

		style->ItemSpacing = { 6.f, 4.f };
		style->ScrollbarSize = 14.f;
		style->GrabMinSize = 12.f;

		style->WindowBorderSize = 1.f;
		style->ChildBorderSize = 1.f;
		style->PopupBorderSize = 1.f;
		style->FrameBorderSize = 1.f;
		style->TabBorderSize = 1.f;

		style->WindowRounding = 3.f;
		style->FrameRounding = 3.f;
		style->PopupRounding = 3.f;
		style->ScrollbarRounding = 3.f;
		style->GrabRounding = 3.f;
		style->TabRounding = 3.f;
	}
}