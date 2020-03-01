// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Rendering/Buffer.h>
#include <Rendering/Texture.h>

#include <string_view>

class RenderGraphPass
{
public:
	RenderGraphPass() = default;
	RenderGraphPass(const RenderGraphPass&) = delete;
	RenderGraphPass(RenderGraphPass&&) noexcept = default;

	RenderGraphPass& operator=(const RenderGraphPass&) = delete;
	RenderGraphPass& operator=(RenderGraphPass&&) noexcept = default;

	size_t CreateResource(const BufferDescription& Description, const std::wstring_view Name);
	size_t CreateResource(const TextureDescription& Description, const std::wstring_view Name);
	void ReadResource(size_t ResourceTag);
	void WriteResource(size_t ResourceTag);
};