// Copyright (c) 2019-2020 Andrew Depke

#pragma once

constexpr auto AlignedSize(size_t Base, size_t Alignment)
{
	return (Base + Alignment - 1) & ~(Alignment - 1);
}