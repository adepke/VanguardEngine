// Copyright (c) 2019-2020 Andrew Depke

#pragma once

enum class OutputBind
{
	RTV,
	DSV
};

class RenderPass
{
	friend class RenderGraph;

private:
	RenderView view;  // #TODO: Can we have multiple views...?

public:
	void SetView(const RenderView& inView);
	void Output(const size_t resourceTag, OutputBind bind);
};