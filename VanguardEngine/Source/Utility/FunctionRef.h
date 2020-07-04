// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <functional>
#include <type_traits>

template <typename>
struct FunctionRef;

template <typename Result, typename... ArgTypes>
struct FunctionRef<Result(ArgTypes...)>
{
private:
	void* object = nullptr;
	Result(*function)(void*, ArgTypes...) = nullptr;

public:
	constexpr FunctionRef() noexcept = delete;
	template <typename Functor>
	constexpr FunctionRef(Functor&& func) noexcept : object(const_cast<void*>(reinterpret_cast<const void*>(std::addressof(func))))
	{
		function = +[](void* object, ArgTypes... args)
		{
			return std::invoke(*reinterpret_cast<std::add_pointer_t<Functor>>(object), std::forward<ArgTypes>(args)...);
		};
	}
	constexpr FunctionRef(const FunctionRef&) noexcept = default;
	constexpr FunctionRef(FunctionRef&&) noexcept = default;

	constexpr FunctionRef& operator=(const FunctionRef&) noexcept = default;
	constexpr FunctionRef& operator=(FunctionRef&&) noexcept = delete;

	template <typename... Ts>
	Result operator()(Ts&&... args) const
	{
		return function(object, std::forward<Ts>(args)...);
	}
};

template <typename Result, typename... ArgTypes>
FunctionRef(Result(*)(ArgTypes...)) -> FunctionRef<Result(ArgTypes...)>;

namespace std
{
	template <typename Result, typename... ArgTypes>
	constexpr void swap(FunctionRef<Result(ArgTypes...)>& left, FunctionRef<Result(ArgTypes...)>& right) noexcept
	{
		std::swap(Left.object, Right.object);
		std::swap(Left.function, Right.function);
	}
}