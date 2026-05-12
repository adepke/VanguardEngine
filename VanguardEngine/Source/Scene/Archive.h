// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <json.hpp>
#include <entt/entt.hpp>

#include <cstdint>
#include <type_traits>
#include <vector>

// Simple JSON-based archiving. EnTT compatible.
// Only supports a single registry.
// Had to separate into two classes because EnTT API
// is unable to distinguish between serialization and
// deserialization in the function operator. Unfortunate.

class ArchiveInput
{
private:
	nlohmann::json entities;  // Array.

public:
	ArchiveInput()
	{
		entities = nlohmann::json::array();
	}

	std::vector<uint8_t> ToBytes()
	{
		// To convert to BSON, the outermost type must be an object.
		// Key by the protocol version.
		auto base = nlohmann::json::object();
		base["v1"] = entities;

		return std::move(nlohmann::json::to_bson(base));
	}

	// EnTT-API functions
	// Three functions are provided:
	// 1. Size of next segment
	// 2. A given entity
	// 3. A given entity with a component

	// Serialize
	void operator()(const std::underlying_type_t<entt::entity> size)
	{
		entities.push_back(size);
	}
	void operator()(const entt::entity entity)
	{
		entities.push_back(static_cast<uint32_t>(entity));
	}
	template <typename T>
	void operator()(const entt::entity entity, const T& t)
	{
		entities.push_back(static_cast<uint32_t>(entity));
		entities.push_back(t);
	}
};

class ArchiveOutput
{
private:
	nlohmann::json entities;  // Array.
	size_t index = 0;  // Tracks the array position when deserializing.

public:
	ArchiveOutput(const char* bytes, size_t size)
	{
		auto base = nlohmann::json::from_bson(bytes, bytes + size);
		entities = base["v1"];
	}

	// EnTT-API functions
	// Three functions are provided:
	// 1. Size of next segment
	// 2. A given entity
	// 3. A given entity with a component

	// Deserialize
	void operator()(std::underlying_type_t<entt::entity>& size)
	{
		size = entities[index].get<std::decay_t<decltype(size)>>();
		++index;
	}
	void operator()(entt::entity& entity)
	{
		entity = static_cast<entt::entity>(entities[index].get<uint32_t>());
		++index;
	}
	template <typename T>
	void operator()(entt::entity& entity, T& t)
	{
		entity = static_cast<entt::entity>(entities[index].get<uint32_t>());
		t = entities[index + 1].get<T>();
		index += 2;
	}
};