// Copyright (c) 2019-2020 Andrew Depke

#pragma once

enum class ResourceBind
{
	CBV,
	SRV,
	UAV
};

class RenderView
{
	size_t Create(TransientBufferDescription description, ResourceBind bind);
	size_t Create(TransientTextureDescription description, ResourceBind bind);
	void Read(size_t resourceTag, ResourceBind bind);
	void Write(size_t resourceTag, ResourceBind bind);
};