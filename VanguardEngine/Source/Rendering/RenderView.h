// Copyright (c) 2019-2020 Andrew Depke

#pragma once

#include <set>

enum class ResourceBind
{
	CBV,
	SRV,
	UAV
};

class RenderView
{
private:
	std::set<size_t> reads;
	std::set<size_t> writes;

public:
	const size_t Create(TransientBufferDescription description, ResourceBind bind);
	const size_t Create(TransientTextureDescription description, ResourceBind bind);
	void Read(const size_t resourceTag, ResourceBind bind);
	void Write(const size_t resourceTag, ResourceBind bind);
};