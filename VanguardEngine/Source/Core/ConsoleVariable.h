// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <Utility/Singleton.h>
#include <Core/Logging.h>
#include <Utility/StringTools.h>

#include <entt/entt.hpp>

#include <string>
#include <unordered_map>
#include <variant>
#include <type_traits>

#define CvarCreate(name, description, defaultValue) CvarManager::Get().CreateVariable(name, description, defaultValue)
#define CvarGet(name, type) CvarManager::Get().GetVariable<type>(name ## _hs)
#define CvarSet(name, value) CvarManager::Get().SetVariable(name ## _hs, value)

using namespace entt::literals;

struct Cvar
{
	enum class CvarType
	{
		Int = 0,
		Float = 1,
		Function = 2
	};

	size_t index;
	CvarType type;
	std::string name;
	std::string description;
};

using CvarCallableType = void (*)();

struct CvarStorage
{
	static constexpr size_t StorageSize = 1000;
	using IntStorage = std::array<int, StorageSize>;
	using FloatStorage = std::array<float, StorageSize>;
	using FunctionStorage = std::array<CvarCallableType, StorageSize>;

	std::variant<IntStorage, FloatStorage, FunctionStorage> buffer;
	size_t count = 0;
};

class CvarManager : public Singleton<CvarManager>
{
	friend class EditorUI;

private:
	std::unordered_map<uint32_t, Cvar> cvars;
	std::array<CvarStorage, 3> storage;

	// Overloads only used by the editor.
	template <typename T>
	bool SetVariable(uint32_t nameHash, const T& value);
	bool ExecuteVariable(uint32_t nameHash);

public:
	template <typename T>
	Cvar& CreateVariable(const std::string& name, const std::string& description, const T& defaultValue);

	template <typename T>
	const T* GetVariable(entt::hashed_string name);

	template <typename T>
	bool SetVariable(entt::hashed_string name, const T& value);

	bool ExecuteVariable(entt::hashed_string name);
};

template <typename T>
inline constexpr Cvar::CvarType FindCvarType()
{
	if constexpr (std::is_same_v<T, int>)
	{
		return Cvar::CvarType::Int;
	}
	else if constexpr (std::is_same_v<T, float>)
	{
		return Cvar::CvarType::Float;
	}
	else if constexpr (std::is_same_v<T, CvarCallableType>)
	{
		return Cvar::CvarType::Function;
	}
	else
	{
		// https://stackoverflow.com/a/64354296
		[]<bool result = false>()
		{
			static_assert(result, __FUNCTION__ ": Type is unrecognized by the console variable system.");
		}();
	}
}

template <typename T>
Cvar& CvarManager::CreateVariable(const std::string& name, const std::string& description, const T& defaultValue)
{
	const auto hash = entt::hashed_string::value(name.data(), name.size());

	auto existing = cvars.find(hash);
	if (existing != cvars.end())
	{
		return existing->second;
	}

	if constexpr (!std::is_same_v<T, CvarCallableType>)
	{
		VGLog(logCore, "Cvar '{}' created with default value: {}", Str2WideStr(name), defaultValue);
	}
	
	else
	{
		VGLog(logCore, "Cvar '{}' created with default value: <function>", Str2WideStr(name));
	}

	const auto type = FindCvarType<std::decay_t<T>>();
	const auto index = storage[(uint32_t)type].count++;

	// First entry in the storage needs to set the variant type.
	if (storage[(uint32_t)type].count == 1)
	{
		storage[(uint32_t)type].buffer = std::array<T, CvarStorage::StorageSize>{};
	}

	const auto it = cvars.emplace(hash, Cvar{
		.index = index,
		.type = type,
		.name = name,
		.description = description
	}).first;

	void* data = nullptr;

	std::visit([index, &data](auto&& buffer)
	{
		data = buffer.data() + index;
	}, storage[(uint32_t)type].buffer);

	*(T*)data = defaultValue;

	return it->second;
}

template <typename T>
const T* CvarManager::GetVariable(entt::hashed_string name)
{
	const auto it = cvars.find(name);
	if (it != cvars.end())
	{
		const auto index = it->second.index;
		const auto type = it->second.type;
		void* data = nullptr;

		std::visit([index, &data](auto&& buffer)
		{
			data = buffer.data() + index;
		}, storage[(uint32_t)type].buffer);

		return (const T*)data;
	}

	return nullptr;
}

template <typename T>
bool CvarManager::SetVariable(entt::hashed_string name, const T& value)
{
	return SetVariable(name.value(), value);
}

template <typename T>
bool CvarManager::SetVariable(uint32_t nameHash, const T& value)
{
	const auto it = cvars.find(nameHash);
	if (it != cvars.end())
	{
		if constexpr (!std::is_same_v<T, CvarCallableType>)
		{
			VGLog(logCore, "Cvar '{}' set to value: {}", Str2WideStr(it->second.name), value);
		}

		else
		{
			VGLog(logCore, "Cvar '{}' set to value: <function>", Str2WideStr(it->second.name));
		}

		const auto index = it->second.index;
		const auto type = it->second.type;
		void* data = nullptr;

		std::visit([index, &data](auto&& buffer)
		{
			data = buffer.data() + index;
		}, storage[(uint32_t)type].buffer);

		*(T*)data = value;
		return true;
	}

	return false;
}

inline bool CvarManager::ExecuteVariable(entt::hashed_string name)
{
	return ExecuteVariable(name.value());
}

inline bool CvarManager::ExecuteVariable(uint32_t nameHash)
{
	const auto it = cvars.find(nameHash);
	if (it != cvars.end())
	{
		VGLog(logCore, "Cvar '{}' executed.", Str2WideStr(it->second.name));

		const auto index = it->second.index;
		const auto type = it->second.type;
		void* data = nullptr;

		std::visit([index, &data](auto&& buffer)
		{
			data = buffer.data() + index;
		}, storage[(uint32_t)type].buffer);

		(*(CvarCallableType*)data)();
		return true;
	}

	return false;
}