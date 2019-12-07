// Copyright (c) 2019 Andrew Depke

#pragma once

#include <cstdlib>
#include <type_traits>

namespace Detail
{
	template <typename>
	struct ArraySizeInternal : std::integral_constant<size_t, 0> {};

	template <typename T, size_t Size>
	struct ArraySizeInternal<T[Size]> : std::integral_constant<size_t, Size> {};
}

template <typename T>
constexpr auto ArraySize(T&&)
{
	return Detail::ArraySizeInternal<std::remove_cv_t<std::remove_reference_t<T>>>::value;
}