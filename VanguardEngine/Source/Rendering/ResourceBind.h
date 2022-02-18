// Copyright (c) 2019-2022 Andrew Depke

#pragma once

enum class ResourceBind
{
	CBV,
	SRV,
	UAV,
	DSV,
	Indirect,
	Common
};

enum class OutputBind
{
	RTV,
	DSV
};