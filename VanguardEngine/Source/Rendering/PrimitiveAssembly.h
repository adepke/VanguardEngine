// Copyright (c) 2019-2021 Andrew Depke

#pragma once

#include <Rendering/Base.h>

#include <span>
#include <variant>
#include <map>
#include <string>
#include <string_view>

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

// Non-owning view of primitive data for a single mesh.
class PrimitiveAssembly
{
	friend class MeshFactory;

	using VertexAttributeView = std::variant<std::span<XMFLOAT2>, std::span<XMFLOAT3>, std::span<XMFLOAT4>>;

	std::span<uint32_t> indexStream;
	std::map<std::string, VertexAttributeView, AttributeSort> vertexStream;

public:
	void AddIndexStream(std::span<uint32_t> stream) { indexStream = stream; }
	template <typename T>
	void AddVertexStream(const std::string_view name, std::span<T> stream) { vertexStream[std::string{ name }] = stream; }

	size_t GetAttributeSize(const std::string_view name) const
	{
		return std::visit([](auto&& arg)
		{
			return arg.size_bytes() / arg.size();
		}, vertexStream.at(std::string{ name }));
	}

	size_t GetAttributeCount(const std::string_view name) const
	{
		return std::visit([](auto&& arg)
		{
			return arg.size();
		}, vertexStream.at(std::string{ name }));
	}

	uint8_t* GetAttributeData(const std::string_view name) const
	{
		return std::visit([](auto&& arg)
		{
			return reinterpret_cast<uint8_t*>(arg.data());
		}, vertexStream.at(std::string{ name }));
	}
};