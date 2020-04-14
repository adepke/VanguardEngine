// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Rendering/Resource.h>

#include <optional>
#include <unordered_set>
#include <unordered_map>

enum class RGUsage
{
	Default,  // Standard usage, used if the resource doesn't qualify for any of the specific usages below.
	RenderTarget,
	DepthStencil,
	BackBuffer,  // Same as render target, used for extracting dependencies.
};

enum RGBufferTypeFlag
{
	VertexBuf = 1 << 0,
	IndexBuf = 1 << 1,
	ConstantBuf = 1 << 2,
};

// Reduced resource descriptions, the render graph will automatically determine the bind flags, access flags, etc.

struct RGBufferDescription
{
	uint32_t BufferTypeFlags;
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

struct ResourceDependencyData
{
	std::unordered_set<size_t> ReadingPasses;  // List of passes that read from this resource.
	std::unordered_set<size_t> WritingPasses;  // List of passes that write to this resource.
};

struct ResourceUsageData
{
	std::unordered_map<size_t, RGUsage> PassUsage;  // Map of pass index to usage.
};