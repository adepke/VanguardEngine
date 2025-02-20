// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <Rendering/Base.h>
#include <Rendering/ResourceBind.h>

#include <unordered_map>
#include <string>
#include <variant>

enum class HeapType
{
	Visible,
	NonVisible
};

struct ShaderResourceViewDescription
{
	struct BufferDesc
	{
		size_t start;
		size_t count;
	};

	struct TextureDesc
	{
		uint32_t firstMip;
		int32_t mipLevels;
		uint32_t mip;
	};

	// Resource type can be inferred from the pass bind info, but we do this anyways
	// for cross validation.
	std::variant<BufferDesc, TextureDesc> data;
	ResourceBind bind;
	HeapType heap;
};

// Holds descriptors generated for a pass.
struct ResourceView
{
	std::unordered_map<std::string, uint32_t> descriptorIndices;
	std::unordered_map<std::string, DescriptorHandle> fullDescriptors;
};

// Resource descriptors requested by a pass.
struct ResourceViewRequest
{
	std::unordered_map<std::string, ShaderResourceViewDescription> descriptorRequests;
};

struct BufferView : public ResourceViewRequest
{
	BufferView& SRV(const std::string& name, size_t start = 0, size_t count = 0, HeapType heap = HeapType::Visible);
	BufferView& UAV(const std::string& name, size_t start = 0, size_t count = 0, HeapType heap = HeapType::Visible);
};

struct TextureView : public ResourceViewRequest
{
	TextureView& SRV(const std::string& name, uint32_t firstMip = 0, int32_t mipLevels = -1, HeapType heap = HeapType::Visible);
	TextureView& UAV(const std::string& name, uint32_t mip, HeapType heap = HeapType::Visible);
};

inline BufferView& BufferView::SRV(const std::string& name, size_t start, size_t count, HeapType heap)
{
	VGAssert(!descriptorRequests.contains(name), "Buffer view descriptor with name %s already exists!", name.data());
	ShaderResourceViewDescription::BufferDesc desc;
	desc.start = start;
	desc.count = count;
	descriptorRequests[name] = ShaderResourceViewDescription{ desc, ResourceBind::SRV, heap };

	return *this;
}

inline BufferView& BufferView::UAV(const std::string& name, size_t start, size_t count, HeapType heap)
{
	VGAssert(!descriptorRequests.contains(name), "Buffer view descriptor with name %s already exists!", name.data());
	ShaderResourceViewDescription::BufferDesc desc;
	desc.start = start;
	desc.count = count;
	descriptorRequests[name] = ShaderResourceViewDescription{ desc, ResourceBind::UAV, heap };

	return *this;
}

inline TextureView& TextureView::SRV(const std::string& name, uint32_t firstMip, int32_t mipLevels, HeapType heap)
{
	VGAssert(!descriptorRequests.contains(name), "Texture view descriptor with name %s already exists!", name.data());
	ShaderResourceViewDescription::TextureDesc desc;
	desc.firstMip = firstMip;
	desc.mipLevels = mipLevels;
	descriptorRequests[name] = ShaderResourceViewDescription{ desc, ResourceBind::SRV, heap };

	return *this;
}

inline TextureView& TextureView::UAV(const std::string& name, uint32_t mip, HeapType heap)
{
	VGAssert(!descriptorRequests.contains(name), "Texture view descriptor with name %s already exists!", name.data());
	ShaderResourceViewDescription::TextureDesc desc;
	desc.mip = mip;
	descriptorRequests[name] = ShaderResourceViewDescription{ desc, ResourceBind::UAV, heap };

	return *this;
}