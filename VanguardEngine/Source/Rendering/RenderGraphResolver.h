// Copyright (c) 2019-2020 Andrew Depke

#pragma once

// Helper type to resolve resource tags into actual resource handles.
class RGResolver
{
public:
	template <typename T>
	T& Get(size_t ResourceTag)
	{
		// #TODO: Resolve the resource via the render graph.
	}
};