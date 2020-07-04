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
	uint32_t bufferTypeFlags;
	ResourceFrequency updateRate = ResourceFrequency::Dynamic;
	size_t size;  // Element count. Size * Stride = Byte count.
	size_t stride = 0;
	std::optional<DXGI_FORMAT> format;
};

struct RGTextureDescription
{
	uint32_t width = 1;
	uint32_t height = 1;
	uint32_t depth = 1;
	DXGI_FORMAT format;
};

struct ResourceDependencyData
{
	std::unordered_set<size_t> readingPasses;  // List of passes that read from this resource.
	std::unordered_set<size_t> writingPasses;  // List of passes that write to this resource.
};

struct ResourceUsageData
{
	std::unordered_map<size_t, RGUsage> passUsage;  // Map of pass index to usage.
};