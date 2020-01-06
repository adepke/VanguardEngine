// Copyright (c) 2019-2020 Andrew Depke

#include <Rendering/CommandList.h>

void CommandList::Create(D3D12_COMMAND_LIST_TYPE Type)
{
	auto Result = Device->CreateCommandAllocator(Type, IID_PPV_ARGS(Allocator.Indirect()));
	if (FAILED(Result))
	{
		VGLogFatal(Rendering) << "Failed to create command allocator: " << Result;
	}

	Result = Device->CreateCommandList(0, Type, Allocator.Get(), nullptr, IID_PPV_ARGS(List.Indirect()));
	if (FAILED(Result))
	{
		VGLogFatal(Rendering) << "Failed to create command list: " << Result;
	}
}