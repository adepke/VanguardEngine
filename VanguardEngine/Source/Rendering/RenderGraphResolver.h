// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <Rendering/Buffer.h>
#include <Rendering/Texture.h>
#include <Rendering/RenderGraphResource.h>

#include <unordered_map>
#include <memory>
#include <utility>
#include <type_traits>

class RenderDevice;

// Helper type to resolve resource tags into actual resource handles.
class RGResolver
{
private:
	size_t Counter = 0;
	std::unordered_map<size_t, std::shared_ptr<Buffer>> BufferResources;
	std::unordered_map<size_t, std::shared_ptr<Texture>> TextureResources;

	std::unordered_map<size_t, std::pair<RGBufferDescription, std::wstring>> TransientBufferResources;
	std::unordered_map<size_t, std::pair<RGTextureDescription, std::wstring>> TransientTextureResources;

public:  // Utilities for the render graph.
	size_t AddResource(const std::shared_ptr<Buffer>& Resource) { const auto Tag = Counter++; BufferResources[Tag] = Resource; return Tag; }
	size_t AddResource(const std::shared_ptr<Texture>& Resource) { const auto Tag = Counter++; TextureResources[Tag] = Resource; return Tag; }

	size_t AddTransientResource(const RGBufferDescription& Description) { const auto Tag = Counter++; TransientBufferResources[Tag].first = Description; return Tag; }
	size_t AddTransientResource(const RGTextureDescription& Description) { const auto Tag = Counter++; TransientTextureResources[Tag].first = Description; return Tag; }

	void NameTransientBuffer(size_t ResourceTag, const std::wstring& Name) { TransientBufferResources[ResourceTag].second = Name; }
	void NameTransientTexture(size_t ResourceTag, const std::wstring& Name) { TransientTextureResources[ResourceTag].second = Name; }

	void BuildTransients(RenderDevice* Device, std::unordered_map<size_t, ResourceDependencyData>& Dependencies, std::unordered_map<size_t, ResourceUsageData>& Usages);

	D3D12_RESOURCE_STATES DetermineInitialState(size_t ResourceTag);

	inline std::shared_ptr<Buffer> FetchAsBuffer(size_t ResourceTag);
	inline std::shared_ptr<Texture> FetchAsTexture(size_t ResourceTag);

public:
	template <typename T>
	T& Get(size_t ResourceTag)
	{
		if constexpr (std::is_same_v<T, Buffer>)
		{
			VGAssert(BufferResources.count(ResourceTag), "Failed to resolve the resource as a buffer.");		
			return *BufferResources[ResourceTag];
		}

		else if constexpr (std::is_same_v<T, Texture>)
		{
			VGAssert(TextureResources.count(ResourceTag), "Failed to resolve the resource as a texture.");
			return *TextureResources[ResourceTag];
		}

		else
		{
			static_assert(false, "Graph resolver can only accept types 'Buffer' or 'Texture'");
			return {};
		}
	}
};

inline std::shared_ptr<Buffer> RGResolver::FetchAsBuffer(size_t ResourceTag)
{
	if (BufferResources.count(ResourceTag))
		return BufferResources[ResourceTag];

	return {};
}

inline std::shared_ptr<Texture> RGResolver::FetchAsTexture(size_t ResourceTag)
{
	if (TextureResources.count(ResourceTag))
		return TextureResources[ResourceTag];

	return {};
}