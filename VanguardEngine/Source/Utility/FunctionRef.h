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
	void* Object = nullptr;
	Result(*Function)(void*, ArgTypes...) = nullptr;

public:
	constexpr FunctionRef() noexcept = delete;
	template <typename Functor>
	constexpr FunctionRef(Functor&& Func) noexcept : Object(const_cast<void*>(reinterpret_cast<const void*>(std::addressof(Func))))
	{
		Function = +[](void* Object, ArgTypes... Args)
		{
			return std::invoke(*reinterpret_cast<std::add_pointer_t<Functor>>(Object), std::forward<ArgTypes>(Args)...);
		};
	}
	constexpr FunctionRef(const FunctionRef&) noexcept = default;
	constexpr FunctionRef(FunctionRef&&) noexcept = default;

	constexpr FunctionRef& operator=(const FunctionRef&) noexcept = default;
	constexpr FunctionRef& operator=(FunctionRef&&) noexcept = delete;

	template <typename... Ts>
	Result operator()(Ts&&... Args) const
	{
		return Function(Object, std::forward<Ts>(Args)...);
	}
};

template <typename Result, typename... ArgTypes>
FunctionRef(Result(*)(ArgTypes...)) -> FunctionRef<Result(ArgTypes...)>;

namespace std
{
	template <typename Result, typename... ArgTypes>
	constexpr void swap(FunctionRef<Result(ArgTypes...)>& Left, FunctionRef<Result(ArgTypes...)>& Right) noexcept
	{
		std::swap(Left.Object, Right.Object);
		std::swap(Left.Function, Right.Function);
	}
}