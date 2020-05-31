// Copyright (c) 2019-2020 Andrew Depke

#pragma once

template <typename T>
struct Singleton
{
	static inline T& Get() noexcept
	{
		static T Instance;
		return Instance;
	}

protected:
	Singleton() = default;
	Singleton(const Singleton&) = delete;
	Singleton(Singleton&&) noexcept = delete;

	Singleton& operator=(const Singleton&) = delete;
	Singleton& operator=(Singleton&&) noexcept = delete;
};