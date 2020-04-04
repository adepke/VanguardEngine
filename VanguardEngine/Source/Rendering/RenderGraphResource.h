// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Rendering/Resource.h>

#include <optional>

enum class RGUsage
{
	Default,  // Standard usage, used if the resource doesn't qualify for any of the specific usages below.
	RenderTarget,
	DepthStencil,
	BackBuffer,  // Same as render target, used for extracting dependencies.
};

enum RGUsageFlag
{
	VertexBuffer = 1 << 0,
	IndexBuffer = 1 << 1,
	ConstantBuffer = 1 << 2,
};

// Reduced resource descriptions, the render graph will automatically determine the bind flags, access flags, etc.

struct RGBufferDescription
{
	uint32_t UsageFlags;
	ResourceFrequency UpdateRate = ResourceFrequency::Dynamic;
	size_t Size;  // Element count. Size * Stride = Byte count.
	size_t Stride = 0;
	std::optional<DXGI_FORMAT> Format;
};

struct RGTextureDescription
{
	uint32_t Width = 1;
	uint32_t Height = 1;
	uint32_t Depth = 1;
	DXGI_FORMAT Format;
};