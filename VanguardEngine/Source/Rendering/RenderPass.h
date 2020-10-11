// Copyright (c) 2019-2020 Andrew Depke

#pragma once

enum class OutputBind
{
	RTV,
	DSV
};

class RenderPass
{
	void SetView(const RenderView& view);
	void Output(size_t resourceTag, OutputBind bind);
};