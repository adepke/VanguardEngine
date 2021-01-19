// Copyright (c) 2019-2021 Andrew Depke

#pragma once

#include <Rendering/Device.h>
#include <Rendering/Resource.h>
#include <Rendering/ResourceManager.h>

#include <imgui.h>

namespace ImGui
{
	void Image(RenderDevice* device, TextureHandle handle, float scale = 1.f)
	{
		if (!device->GetResourceManager().Valid(handle)) return;

		const auto& textureComponent = device->GetResourceManager().Get(handle);
		ImGui::Image((ImTextureID)textureComponent.SRV->bindlessIndex, { (float)textureComponent.description.width * scale, (float)textureComponent.description.height * scale });
	}
}