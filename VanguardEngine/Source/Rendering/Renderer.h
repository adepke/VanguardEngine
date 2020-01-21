// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Core/Base.h>

#include <entt/entt.hpp>  // #TODO: Don't include, forward and include in the source.

class RenderDevice;
struct CommandList;

enum class RenderPass
{
	Main,
};

class Renderer
{
public:
	std::shared_ptr<RenderDevice> Device;

private:
	auto GetPassRenderTargets(RenderPass Pass);
	void BeginRenderPass(RenderPass Pass);
	void EndRenderPass(RenderPass Pass);
	void SetDescriptorHeaps(CommandList& List);

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

	void Initialize(std::unique_ptr<RenderDevice>&& InDevice);

	// Entity data is safe to write to immediately after this function returns. Do not attempt to write before Render() returns.
	void Render(entt::registry& Registry);
};