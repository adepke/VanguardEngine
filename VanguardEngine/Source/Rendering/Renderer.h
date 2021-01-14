// Copyright (c) 2019-2021 Andrew Depke

#pragma once

#include <Core/Base.h>
#include <Utility/Singleton.h>
#include <Window/WindowFrame.h>
#include <Rendering/Device.h>
#include <Rendering/PipelineBuilder.h>
#include <Rendering/Material.h>
#include <Rendering/UserInterface.h>
#include <Rendering/DescriptorAllocator.h>

#include <entt/entt.hpp>

// #TODO: Fix Windows.h leaking.
#include <Rendering/Resource.h>

class CommandList;

class Renderer : public Singleton<Renderer>
{
public:
	std::unique_ptr<WindowFrame> window;
	std::unique_ptr<RenderDevice> device;  // Destruct the device after all other resources.

private:
	PipelineBuilder pipelines;  // Manages all the pipelines.

	BufferHandle cameraBuffer;
	std::unique_ptr<UserInterfaceManager> userInterface;

	DescriptorHandle nullDescriptor;

private:
	void UpdateCameraBuffer();
	void CreatePipelines();
	std::pair<BufferHandle, size_t> CreateInstanceBuffer(const entt::registry& registry);

public:
	~Renderer();

	void Initialize(std::unique_ptr<WindowFrame>&& inWindow, std::unique_ptr<RenderDevice>&& inDevice);

	// Entity data is safe to write to immediately after this function returns. Do not attempt to write before Render() returns.
	void Render(entt::registry& registry);
};