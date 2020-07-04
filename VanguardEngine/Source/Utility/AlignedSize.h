// Copyright (c) 2019-2020 Andrew Depke

#pragma once

constexpr auto AlignedSize(size_t base, size_t alignment)
{
	return (base + alignment - 1) & ~(alignment - 1);
}