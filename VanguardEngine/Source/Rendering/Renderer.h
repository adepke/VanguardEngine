// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Core/Base.h>
#include <Utility/Singleton.h>
#include <Window/WindowFrame.h>
#include <Rendering/Device.h>
#include <Rendering/PipelineState.h>
#include <Rendering/Material.h>
#include <Rendering/Buffer.h>
#include <Rendering/Texture.h>
#include <Rendering/UserInterface.h>
#include <Rendering/DescriptorAllocator.h>

#include <entt/entt.hpp>

class CommandList;

class Renderer : public Singleton<Renderer>
{
public:
	std::unique_ptr<WindowFrame> Window;
	std::unique_ptr<RenderDevice> Device;  // Destruct the device after all other resources.

private:
	std::shared_ptr<Buffer> cameraBuffer;
	std::vector<Material> Materials;
	std::unique_ptr<UserInterfaceManager> UserInterface;

	DescriptorHandle NullDescriptor;

private:
	void UpdateCameraBuffer();

public:
	~Renderer();

	void Initialize(std::unique_ptr<WindowFrame>&& InWindow, std::unique_ptr<RenderDevice>&& InDevice);

	// Entity data is safe to write to immediately after this function returns. Do not attempt to write before Render() returns.
	void Render(entt::registry& Registry);
};