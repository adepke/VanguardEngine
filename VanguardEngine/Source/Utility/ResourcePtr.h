// Copyright (c) 2019 Andrew Depke

#pragma once

#include <type_traits>
#include <memory>
#include <functional>

namespace Detail
{
	template <typename T>
	struct Releasable
	{
		typedef char(&Yes)[1];
		typedef char(&No)[2];

		template <typename U>
		static constexpr Yes Check(decltype(&U::Release));
		template <typename>
		static constexpr No Check(...);

		static constexpr bool Value = sizeof(Check<T>(0)) == sizeof(Yes);
	};

	constexpr auto ReleasableDeleter = [](auto* Ptr) { Ptr->Release(); delete Ptr; };

	template <typename T, bool>
	struct ResourcePtrInternal : public std::unique_ptr<T>
	{
		using BaseType = std::unique_ptr<T>;

		using BaseType::BaseType;
	};

	template <typename T>
	struct ResourcePtrInternal<T, true> : std::unique_ptr<T, std::function<void(T*)>>
	{
		using BaseType = std::unique_ptr<T, std::function<void(T*)>>;

		constexpr ResourcePtrInternal() noexcept : BaseType(nullptr, ReleasableDeleter) {}
		ResourcePtrInternal(T* Ptr) noexcept : BaseType(Ptr, ReleasableDeleter) {}

		using BaseType::BaseType;
	};
}

template <typename T>
using ResourcePtr = Detail::ResourcePtrInternal<T, Detail::Releasable<T>::Value>;