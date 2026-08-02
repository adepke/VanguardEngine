// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <Rendering/Base.h>

#include <span>
#include <map>
#include <string>
#include <string_view>
#include <algorithm>
#include <cstdint>

struct AttributeSort
{
	static inline const std::string order[] = {
		"POSITION",
		"NORMAL",
		"TEXCOORD_0",
		"TANGENT",
		"BITANGENT",
		"COLOR_0"
	};

	bool operator()(const std::string& left, const std::string& right) const
	{
		const auto leftIt = std::find(std::cbegin(order), std::cend(order), left);
		const auto rightIt = std::find(std::cbegin(order), std::cend(order), right);

		if (leftIt == std::cend(order) && rightIt == std::cend(order))
			return left < right;  // Fallback to lexicographical sort.
		else
			return leftIt < rightIt;
	}
};

// Non-owning view of primitive data for a single mesh. Streams are not necessarily tightly packed.
class PrimitiveAssembly
{
	friend class MeshFactory;

	struct VertexAttributeView
	{
		const uint8_t* data = nullptr;
		size_t count = 0;
		size_t size = 0;
		size_t stride = 0;
	};

	std::span<uint32_t> indexStream;
	std::map<std::string, VertexAttributeView, AttributeSort> vertexStream;

public:
	void AddIndexStream(std::span<uint32_t> stream) { indexStream = stream; }

	// Tightly packed source.
	template <typename T>
	void AddVertexStream(const std::string_view name, std::span<T> stream)
	{
		vertexStream[std::string{ name }] = VertexAttributeView{
			.data = reinterpret_cast<const uint8_t*>(stream.data()),
			.count = stream.size(),
			.size = sizeof(T),
			.stride = sizeof(T)
		};
	}

	// Interleaved source.
	template <typename T>
	void AddVertexStream(const std::string_view name, const T* first, size_t count, size_t stride)
	{
		vertexStream[std::string{ name }] = VertexAttributeView{
			.data = reinterpret_cast<const uint8_t*>(first),
			.count = count,
			.size = sizeof(T),
			.stride = stride
		};
	}

	const std::span<uint32_t> GetIndexStream() const
	{
		return indexStream;
	}

	size_t GetAttributeSize(const std::string_view name) const
	{
		return vertexStream.at(std::string{ name }).size;
	}

	size_t GetAttributeCount(const std::string_view name) const
	{
		return vertexStream.at(std::string{ name }).count;
	}

	size_t GetAttributeStride(const std::string_view name) const
	{
		return vertexStream.at(std::string{ name }).stride;
	}

	const uint8_t* GetAttributeData(const std::string_view name) const
	{
		return vertexStream.at(std::string{ name }).data;
	}
};
