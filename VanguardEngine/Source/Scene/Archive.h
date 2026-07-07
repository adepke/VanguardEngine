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

// Version history:
// v1: EnTT v3.8.1
// v2: EnTT v3.16.0

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
		base["v2"] = entities;

		return std::move(nlohmann::json::to_bson(base));
	}

	// EnTT-API functions
	// Three functions are provided:
	// 1. Size of next segment
	// 2. A given entity
	// 3. A given component

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
	void operator()(const T& t)
	{
		entities.push_back(t);
	}
};

class ArchiveOutput
{
private:
	nlohmann::json entities;  // Array.
	int version = -1;  // Which version is being loaded.
	size_t index = 0;  // Tracks the array position when deserializing.

	void ConvertV1()
	{
		// Entities layout:
		// v1 (basic_snapshot::entities):    [length, slot0, slot1, ..., slotN-1, destroyed]
		// v2 (basic_snapshot::get<entity>): [length, aliveCount, e0, e1, ..., eN-1]
		// Components are unchanged.

		using Traits = entt::entt_traits<entt::entity>;
		using EntityInt = std::underlying_type_t<entt::entity>;

		if (!entities.is_array() || entities.empty())
			return;

		const auto length = entities[0].get<EntityInt>();
		const size_t destroyedPos = static_cast<size_t>(1) + length;
		if (destroyedPos >= entities.size())
			return;  // Malformed

		const auto nullPart = Traits::to_entity(static_cast<entt::entity>(entt::null));

		std::vector<EntityInt> slots(length);
		for (EntityInt i = 0; i < length; ++i)
			slots[i] = entities[static_cast<size_t>(1) + i].get<EntityInt>();
		const EntityInt destroyedHead = entities[destroyedPos].get<EntityInt>();

		// Walk the intrusive free list to mark released slots.
		std::vector<bool> released(length, false);
		for (EntityInt cursor = destroyedHead; entt::to_entity(cursor) != nullPart; )
		{
			const auto slot = entt::to_entity(cursor);
			released[slot] = true;
			cursor = slots[slot];
		}

		// Rebuild the segment in v2 layout: [length, aliveCount, alive..., released...].
		nlohmann::json rebuilt = nlohmann::json::array();
		rebuilt.push_back(length);
		rebuilt.push_back(EntityInt{ 0 });  // aliveCount, patched below.

		EntityInt aliveCount = 0;
		for (EntityInt i = 0; i < length; ++i)
		{
			if (!released[i])
			{
				rebuilt.push_back(slots[i]);  // Alive slot already encodes (id, version).
				++aliveCount;
			}
		}
		for (EntityInt i = 0; i < length; ++i)
		{
			if (released[i])
			{
				// Released slots store a free-list pointer in their entity part; rewrite
				// them back into a plain identifier keyed by slot index.
				rebuilt.push_back(Traits::construct(i, entt::to_version(slots[i])));
			}
		}
		rebuilt[1] = aliveCount;

		// Splice the rebuilt entity segment back in, replacing the original one.
		entities.erase(entities.begin(), entities.begin() + (destroyedPos + 1));
		entities.insert(entities.begin(), rebuilt.begin(), rebuilt.end());
	}

public:
	ArchiveOutput(const char* bytes, size_t size)
	{
		auto base = nlohmann::json::from_bson(bytes, bytes + size);

		if (base.contains("v1"))
			version = 1;
		else if (base.contains("v2"))
			version = 2;

		if (version > 0)
		{
			entities = base[std::format("v{}", version)];
		}

		// Upgrade old versions so they can be loaded.
		switch (version)
		{
		case 1:
			ConvertV1();
			break;
		}
	}

	bool Valid() const { return version > 0; }

	// EnTT-API functions
	// Three functions are provided:
	// 1. Size of next segment
	// 2. A given entity
	// 3. A given component

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
	void operator()(T& t)
	{
		t = entities[index].get<T>();
		++index;
	}
};
