// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Core/Base.h>

#include <entt/entt.hpp>  // #TODO: Don't include, forward and include in the source.

class RenderDevice;
struct ID3D12GraphicsCommandList;

class Renderer
{
public:
	std::unique_ptr<RenderDevice> Device;

private:
	void SetRenderTargets(ID3D12GraphicsCommandList* CommandList, size_t Frame);

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