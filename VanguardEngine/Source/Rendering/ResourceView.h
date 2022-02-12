// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <Rendering/Base.h>
#include <Rendering/DescriptorHeap.h>

#include <unordered_map>
#include <string>
#include <variant>

struct ShaderResourceViewDescription
{
	enum class Bind
	{
		SRV,
		UAV
	};

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
	Bind bind;
};

// Holds descriptor ids generated for a pass.
struct ResourceView
{
	std::unordered_map<std::string, uint32_t> descriptors;
};

// Resource descriptors requested by a pass.
struct ResourceViewRequest
{
	std::unordered_map<std::string, ShaderResourceViewDescription> descriptorRequests;
};

struct BufferView : public ResourceViewRequest
{
	BufferView& SRV(const std::string& name, size_t start, size_t count);
	BufferView& UAV(const std::string& name, size_t start, size_t count);
};

struct TextureView : public ResourceViewRequest
{
	TextureView& SRV(const std::string& name, uint32_t firstMip, int32_t mipLevels);
	TextureView& UAV(const std::string& name, uint32_t mip);
};

inline BufferView& BufferView::SRV(const std::string& name, size_t start, size_t count)
{
	VGAssert(!descriptorRequests.contains(name), "Buffer view descriptor with name %s already exists!", name.data());
	ShaderResourceViewDescription::BufferDesc desc;
	desc.start = start;
	desc.count = count;
	descriptorRequests[name] = ShaderResourceViewDescription{ desc, ShaderResourceViewDescription::Bind::SRV };

	return *this;
}

inline BufferView& BufferView::UAV(const std::string& name, size_t start, size_t count)
{
	VGAssert(!descriptorRequests.contains(name), "Buffer view descriptor with name %s already exists!", name.data());
	ShaderResourceViewDescription::BufferDesc desc;
	desc.start = start;
	desc.count = count;
	descriptorRequests[name] = ShaderResourceViewDescription{ desc, ShaderResourceViewDescription::Bind::UAV };

	return *this;
}

inline TextureView& TextureView::SRV(const std::string& name, uint32_t firstMip, int32_t mipLevels)
{
	VGAssert(!descriptorRequests.contains(name), "Texture view descriptor with name %s already exists!", name.data());
	ShaderResourceViewDescription::TextureDesc desc;
	desc.firstMip = firstMip;
	desc.mipLevels = mipLevels;
	descriptorRequests[name] = ShaderResourceViewDescription{ desc, ShaderResourceViewDescription::Bind::SRV };

	return *this;
}

inline TextureView& TextureView::UAV(const std::string& name, uint32_t mip)
{
	VGAssert(!descriptorRequests.contains(name), "Texture view descriptor with name %s already exists!", name.data());
	ShaderResourceViewDescription::TextureDesc desc;
	desc.mip = mip;
	descriptorRequests[name] = ShaderResourceViewDescription{ desc, ShaderResourceViewDescription::Bind::UAV };

	return *this;
}