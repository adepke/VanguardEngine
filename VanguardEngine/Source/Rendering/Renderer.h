// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Core/Base.h>
#include <Rendering/PipelineState.h>
#include <Rendering/Material.h>
#include <Rendering/Buffer.h>
#include <Rendering/Texture.h>
#include <Rendering/UserInterface.h>

#include <entt/entt.hpp>  // #TODO: Don't include, forward and include in the source.

class RenderDevice;
class CommandList;

class Renderer
{
public:
	// Destruct the device after all other resources.
	std::unique_ptr<RenderDevice> Device;

private:
	std::shared_ptr<Buffer> cameraBuffer;
	std::vector<Material> Materials;
	std::unique_ptr<UserInterfaceManager> UserInterface;

private:
	void UpdateCameraBuffer();

public:
	static inline Renderer& Get() noexcept
	{
		static Renderer Singleton;
		return Singleton;
	}

	Renderer() = default;
	Renderer(const Renderer&) = delete;
	Renderer(Renderer&&) noexcept = delete;

	Renderer& operator=(const Renderer&) = delete;
	Renderer& operator=(Renderer&&) noexcept = delete;

	~Renderer();

	void Initialize(std::unique_ptr<RenderDevice>&& InDevice);

	// Entity data is safe to write to immediately after this function returns. Do not attempt to write before Render() returns.
	void Render(entt::registry& Registry);
};