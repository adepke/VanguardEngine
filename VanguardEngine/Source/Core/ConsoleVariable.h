// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <Utility/Singleton.h>

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
		Float = 1
	};

	size_t index;
	CvarType type;
	std::string name;
	std::string description;
};

struct CvarStorage
{
	using IntStorage = std::array<int, 1000>;
	using FloatStorage = std::array<float, 1000>;

	std::variant<IntStorage, FloatStorage> buffer;
	size_t count = 0;
};

class CvarManager : public Singleton<CvarManager>
{
private:
	std::unordered_map<uint32_t, Cvar> cvars;
	std::array<CvarStorage, 3> storage;

public:
	template <typename T>
	Cvar& CreateVariable(const std::string& name, const std::string& description, const T& defaultValue);

	template <typename T>
	const T* GetVariable(entt::hashed_string name);

	template <typename T>
	bool SetVariable(entt::hashed_string name, const T& value);
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
	const auto type = FindCvarType<std::decay_t<T>>();
	const auto index = storage[(uint32_t)type].count++;

	const auto hash = entt::hashed_string::value(name.data(), name.size());
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

		*(T*)data = value;
		return true;
	}

	return false;
}